// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "file.hpp"

#include <base/config/target_info.hpp>
#include <base/except/exceptions.hpp>
#include <base/misc/shared_view.hpp>
#include <base/pointers/ref.hpp>

#include <filesystem/vfs.hpp>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <random>

#if !BASE_TARGET_OS_WINDOWS
	#include <fcntl.h>
	#include <sys/stat.h>
#endif

namespace {
	/**
	 * @brief Returns the VFS a virtual path resolves against.
	 */
	Ref<fs::VFS> vfsOf(const fs::FilePath& path) { return path.getVfs().value(); }

	// Validation functions
	void requireDirectory(const fs::File& directory) {
		if (!directory.isDirectory()) CORE_PANIC("Parent is not a directory");
	}

	void requireFile(const fs::File& file) {
		if (!file.isFile()) CORE_PANIC("Path is not a file");
	}

	void requirePhysicalPath(const fs::FilePath& path) {
		if (!path.isPhysical()) CORE_PANIC("Path is not a physical file: " + path.string());
	}

	void requireVirtualPath(const fs::FilePath& path) {
		if (!path.isVirtual()) CORE_PANIC("Path is not a virtual path: " + path.string());
	}

	// Best-effort reason for a failed stream operation. `std::ofstream` reports no error_code, so
	// errno - which the implementation sets when the underlying open or write fails - is all there
	// is. Callers zero errno right before the operation; if it is still zero, the platform told us
	// nothing more than `fallback`.
	std::string streamFailureReason(const fs::FilePath& path, std::string_view fallback) {
		auto reason = errno != 0 ? std::error_code(errno, std::generic_category()).message()
		                         : std::string(fallback);
		return path.string() + ": " + reason;
	}

	// Writes `content` to an open stream and closes it. The close is part of the check: a stream
	// buffers small writes, so a failure such as a full disk often surfaces only when the buffer
	// is flushed on close - and a close left to the destructor swallows it.
	std::expected<void, std::string> writeAndClose(
		std::ofstream& ofs, const fs::FilePath& path, std::string_view content
	) {
		errno = 0;
		ofs << content;
		ofs.close();
		if (!ofs) return std::unexpected(streamFailureReason(path, "could not be written"));
		return {};
	}

	/**
	 * @brief Generates a file or directory path inside the given directory.
	 *
	 * If a custom name is provided, uses it; otherwise, generates a random unique name.
	 * Ensures the resulting path does not already exist in the directory.
	 *
	 * @param directory The parent directory as a File.
	 * @param custom_name Optional custom name for the file or directory
	 * @return The generated path inside the directory, or a message saying the name is taken.
	 */
	std::expected<fs::FilePath, std::string> genPathInDirectory(
		const fs::File& directory, std::string_view custom_name = ""
	) {
		requireDirectory(directory);
		const fs::FilePath dir_path = directory.getFilePath();
		if (custom_name.empty()) return fs::FileManager::randomName(dir_path);

		auto candidate = dir_path / custom_name;
		if (candidate.exists()) {
			return std::unexpected(base::strConcat(
				"cannot create \"",
				custom_name,
				"\": there already is a file or folder with this name in ",
				dir_path.native()
			));
		}
		return candidate;
	}
}

namespace fs {
	File::File(const FilePath& path): path(path) {
		if (!path.exists())
			CORE_PANIC(
				base::strConcat(
					"Cannot create file because the path does not point to an existing file: "
				),
				path.string()
			);

		if (path.isVirtual()) {
			this->type = FileType::Virtual;
			this->category
				= vfsOf(path)->isDirectory(path) ? FileCategory::Directory : FileCategory::File;
		} else {
			this->path     = base::valueOrPanic(path.canonical(), "resolving an existing file");
			this->type     = FileType::Physical;
			this->category = path.isDirectory() ? FileCategory::Directory : FileCategory::File;
		}
	}

	std::expected<File, std::string> File::open(const FilePath& path) {
		if (!path.exists()) return std::unexpected(path.string() + ": no such file or folder");
		if (path.isPhysical() || path.getType() == PathType::Relative) {
			if (auto resolved = path.canonical(); !resolved.has_value())
				return std::unexpected(resolved.error());
		}
		return File(path);
	}

	// --- FileManager static methods ---

	std::expected<File, std::string> FileManager::createPhysicalFile(
		const FilePath& path, std::string_view content, bool allow_overwrite
	) {
		if (!path.isPhysical())
			return std::unexpected("path is not a physical file path: " + path.string());

		auto            abs_path = path.absolute();
		std::error_code ec;
		if (std::filesystem::exists(abs_path.getPath(), ec) && !allow_overwrite)
			return std::unexpected("physical file already exists: " + abs_path.string());

		errno = 0;
		std::ofstream ofs(abs_path.getPath(), allow_overwrite ? std::ios::trunc : std::ios::out);
		if (!ofs)
			return std::unexpected(streamFailureReason(abs_path, "could not be opened for writing"));
		if (auto written = writeAndClose(ofs, abs_path, content); !written)
			return std::unexpected(written.error());
		return abs_path;
	}

	std::expected<File, std::string> FileManager::createPhysicalFolder(
		const FilePath& path, bool allow_overwrite
	) {
		if (!path.isPhysical())
			return std::unexpected("path is not a physical folder path: " + path.string());

		auto            abs_path = path.absolute();
		std::error_code ec;
		if (std::filesystem::exists(abs_path.getPath(), ec)) {
			if (!allow_overwrite)
				return std::unexpected("physical folder already exists: " + abs_path.string());
			std::filesystem::remove_all(abs_path.getPath(), ec);
			if (ec) return std::unexpected(ec.message());
		}
		// The bool result only says whether a directory was created, which is false both for a
		// path that already exists and for a failure; the error code is what distinguishes them.
		std::filesystem::create_directories(abs_path.getPath(), ec);
		if (ec) return std::unexpected(ec.message());
		return abs_path;
	}

	File FileManager::createVirtualFile(
		const FilePath& path, std::string_view content, bool allow_overwrite
	) {
		requireVirtualPath(path);

		auto vfs = vfsOf(path);

		if (vfs->exists(path.getPath())) {
			if (!allow_overwrite) CORE_PANIC("Virtual file already exists: " + path.string());
			if (!vfs->isFile(path.getPath()))
				CORE_PANIC("Path exists but is not a file: " + path.string());
		} else {
			vfs->createFile(path.getPath());
		}
		vfs->writeFile(path.getPath(), content);
		return path;
	}

	File FileManager::createVirtualFolder(const FilePath& path, bool allow_overwrite) {
		requireVirtualPath(path);

		auto vfs = vfsOf(path);

		if (vfs->exists(path.getPath())) {
			if (!allow_overwrite) CORE_PANIC("Virtual folder already exists: " + path.string());
			if (!vfs->isDirectory(path.getPath()))
				CORE_PANIC("Path exists but is not a directory: " + path.string());
			vfs->deleteDirectory(path.getPath(), true);
		}
		vfs->createDirectory(path.getPath());
		return path;
	}

	FilePath FileManager::randomName(
		const FilePath& directory, std::string_view prefix, std::string_view suffix
	) {
		// Thread-local: now that this is public, callers on different threads must not share
		// (and race on) one generator.
		thread_local std::mt19937         rng(std::random_device{}());
		static constexpr std::string_view NAME_CHARS
			= "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPRQSTUVWXYZ";
		std::uniform_int_distribution<size_t> dist(0, NAME_CHARS.length() - 1);

		// This is 64, but usually it will not loop more than once.
		for (size_t try_no = 0; try_no < 64; try_no++) {
			std::string name(prefix);
			for (size_t i = 0; i < RANDOM_NAME_LENGTH; i++) name.push_back(NAME_CHARS[dist(rng)]);
			name.append(suffix);
			auto candidate = directory / name;
			if (!exists(candidate.getPath())) return candidate;
		}
		CORE_PANIC("Couldn't create a new name in: " + directory.absolute().string());
	}

	std::expected<File, std::string> FileManager::createRandomTempDirectory() {
		return createPhysicalFolder(randomName(FilePath::getDefaultTempDirectoryPath()));
	}

	File FileManager::createRandomVirtualDirectory(base::Ref<VFS> vfs) {
		FilePath root(vfs->getRootPath(), vfs);
		auto     rand_path = randomName(root);
		vfs->createDirectory(rand_path.getPath());
		return rand_path;
	}

	File FileManager::createRandomVirtualFile(
		std::string_view content, base::Optional<std::string_view> suffix, base::Ref<VFS> vfs
	) {
		FilePath root(vfs->getRootPath(), vfs);
		auto     rand_path = randomName(root);
		if (suffix.has_value())
			rand_path = FilePath(base::strConcat(rand_path.string(), suffix.value()), vfs);
		return createVirtualFile(rand_path, content);
	}

	std::expected<File, std::string> FileManager::createRandomTempFile(std::string_view content) {
		return createPhysicalFile(randomName(FilePath::getDefaultTempDirectoryPath()), content);
	}

	std::expected<bool, std::string> FileManager::deleteFile(const File& file) {
		if (file.type == FileType::Virtual) return vfsOf(file.path)->deleteFile(file.path);
		return removeEntry(file.path);
	}

	std::expected<bool, std::string> FileManager::deleteFolder(const File& folder, bool force) {
		if (folder.type == FileType::Virtual)
			return vfsOf(folder.path)->deleteDirectory(folder.path, force);
		if (!force) return removeEntry(folder.path);
		std::error_code ec;
		const auto      removed = std::filesystem::remove_all(folder.path.getPath(), ec);
		if (ec) return std::unexpected(folder.path.string() + ": " + ec.message());
		return removed > 0;
	}

	base::Optional<FilePath> FileManager::temporaryFileTarget(const FilePath& path) {
		constexpr std::string_view SUFFIX = ".tmp";
		const auto                 name   = path.getPath().filename().string();
		if (!name.ends_with(SUFFIX)) return {};
		const std::string_view without_suffix(name.data(), name.size() - SUFFIX.size());
		const auto             dot = without_suffix.rfind('.');
		if (dot == std::string_view::npos || dot == 0) return {};
		const auto random = without_suffix.substr(dot + 1);
		const bool is_random
			= random.size() == RANDOM_NAME_LENGTH && std::ranges::all_of(random, [](char c) {
				  return std::isalnum(static_cast<unsigned char>(c)) != 0;
			  });
		if (!is_random) return {};
		return path.parentPath() / FilePath(std::string(without_suffix.substr(0, dot)));
	}

	std::expected<void, std::string> FileManager::produceFileAtomically(
		const FilePath& path, const FileProducer& produce
	) {
		// On disk, but relative paths (the default `./duck_build/`) are fine.
		const auto abs_path = path.absolute();
		requirePhysicalPath(abs_path);
		const auto tmp_path = randomName(
			abs_path.parentPath(), abs_path.getPath().filename().string() + ".", ".tmp"
		);
		const auto discard_tmp = [&tmp_path] {
			std::error_code ec;
			std::filesystem::remove(tmp_path.getPath(), ec);
		};

		// Only a fully produced temporary file earns the rename; anything else is thrown away,
		// which is what keeps the previous file intact.
		if (auto produced = produce(tmp_path); !produced.has_value()) {
			discard_tmp();
			return produced;
		}

		std::error_code ec;
		std::filesystem::rename(tmp_path.getPath(), abs_path.getPath(), ec);
		if (ec) {
			discard_tmp();
			return std::unexpected(abs_path.string() + ": " + ec.message());
		}
		return {};
	}

	std::expected<void, std::string> FileManager::writeFileAtomically(
		const FilePath& path, const std::function<void(std::ostream&)>& write_content
	) {
		return produceFileAtomically(
			path,
			[&](const FilePath& tmp_path) -> std::expected<void, std::string> {
				errno = 0;
				std::ofstream file(tmp_path.getPath(), std::ios::binary | std::ios::trunc);
				if (!file)
					return std::unexpected(streamFailureReason(tmp_path, "could not be opened"));
				errno = 0;
				write_content(file);
				file.close();
				if (!file)
					return std::unexpected(streamFailureReason(tmp_path, "could not be written"));
				return {};
			}
		);
	}

	std::expected<bool, std::string> FileManager::removeEntry(const FilePath& path) {
		const auto abs_path = path.absolute();
		requirePhysicalPath(abs_path);
		std::error_code ec;
		const bool      removed = std::filesystem::remove(abs_path.getPath(), ec);
		if (ec) return std::unexpected(abs_path.string() + ": " + ec.message());
		return removed;
	}

	std::expected<void, std::string> FileManager::touch(const FilePath& path) {
		const auto abs_path = path.absolute();
		requirePhysicalPath(abs_path);
#if BASE_TARGET_OS_WINDOWS
		// Setting the time on Windows needs only the right to write the file's attributes.
		std::error_code ec;
		std::filesystem::last_write_time(
			abs_path.getPath(), std::filesystem::file_time_type::clock::now(), ec
		);
		if (ec) return std::unexpected(abs_path.string() + ": " + ec.message());
#else
		// "Now" is passed as no time at all, as the `touch` command does it: POSIX then needs
		// only write permission. `std::filesystem::last_write_time` passes an explicit time,
		// which only the file's owner may set.
		if (utimensat(AT_FDCWD, abs_path.getPath().c_str(), nullptr, 0) != 0) {
			// Saved first: building the message below may run code that sets `errno`.
			const int error = errno;
			return std::unexpected(
				abs_path.string() + ": " + std::error_code(error, std::generic_category()).message()
			);
		}
#endif
		return {};
	}

	// --- File methods (non-static, formerly FilePath) ---

	std::expected<base::SharedView, std::string> File::getContent() const {
		if (type == FileType::Virtual) {
			if (!exists()) return std::unexpected(path.string() + ": no such virtual file");
			auto content = vfsOf(path)->readFile(path);

			auto r_array = new byte[content.size()];
			std::ranges::copy(content, reinterpret_cast<char*>(r_array));

			return base::SharedView{ r_array, content.size() };
		}

		errno = 0;
		std::ifstream file(path.getPath(), std::ios::in | std::ios::binary);
		if (!file) return std::unexpected(streamFailureReason(path, "could not be opened"));

		file.unsetf(std::ios::skipws);
		file.seekg(0, std::ios::end);
		const auto end = file.tellg();
		if (end < 0) return std::unexpected(streamFailureReason(path, "could not be read"));
		const auto file_size = static_cast<usize>(end);
		file.seekg(0, std::ios::beg);

		auto r_array = new byte[file_size];
		file.read(reinterpret_cast<char*>(r_array), std::streamsize(file_size));
		if (file.gcount() != std::streamsize(file_size)) {
			delete[] r_array;
			return std::unexpected(streamFailureReason(path, "could not be read"));
		}
		return base::SharedView{ r_array, file_size };
	}

	bool File::exists() const { return path.exists(); }

	std::string File::name() const { return path.name(); }

	std::string File::stem() const { return path.stem(); }

	bool File::isTemporary() const { return path.isTemporary(); }

	bool File::isDirectory() const noexcept { return category == FileCategory::Directory; }

	bool File::isFile() const noexcept { return !isDirectory(); }

	std::string File::extension() const { return path.extension(); }

	std::expected<void, std::string> File::writeToFile(std::string_view new_content, bool append)
		const {
		requireFile(*this);

		if (type == FileType::Virtual) {
			auto vfs = vfsOf(path);
			if (!vfs->exists(path))
				return std::unexpected("virtual file does not exist: " + path.string());
			vfs->writeFile(path, new_content, append);
			return {};
		}

		errno                   = 0;
		std::ios::openmode mode = append ? (std::ios::out | std::ios::app) : std::ios::out;
		std::ofstream      ofs(path.getPath(), mode);
		if (!ofs)
			return std::unexpected(streamFailureReason(path, "could not be opened for writing"));
		return writeAndClose(ofs, path, new_content);
	}

	std::expected<File, std::string> File::createSubFile(
		std::string_view new_file_content, std::string_view custom_name
	) const {
		requireDirectory(*this);
		auto file_path = genPathInDirectory(*this, custom_name);
		if (!file_path.has_value()) return std::unexpected(file_path.error());

		switch (type) {
		case FileType::Virtual:
			return FileManager::createVirtualFile(*file_path, new_file_content);
		case FileType::Physical:
			return FileManager::createPhysicalFile(*file_path, new_file_content);
		default:
			CORE_PANIC("Unsupported directory type for file creation");
		}
	}

	std::expected<File, std::string> File::createSubDirectory(std::string_view custom_name) const {
		requireDirectory(*this);
		auto dir_path = genPathInDirectory(*this, custom_name);
		if (!dir_path.has_value()) return std::unexpected(dir_path.error());

		switch (type) {
		case FileType::Virtual:
			return FileManager::createVirtualFolder(*dir_path);
		case FileType::Physical:
			return FileManager::createPhysicalFolder(*dir_path);
		default:
			CORE_PANIC("Unsupported directory type for directory creation");
		}
	}

	std::expected<std::vector<FilePath>, std::string> File::listFilePaths() const {
		if (!isDirectory()) return std::unexpected(path.string() + ": not a directory");

		std::vector<FilePath> result;
		if (type == FileType::Virtual) {
			for (const auto& entry: vfsOf(path)->listDirectory(path))
				result.emplace_back(path / entry);
			return result;
		}

		std::error_code ec;
		for (std::filesystem::directory_iterator it(path.getPath(), ec), end; !ec && it != end;
		     it.increment(ec))
			result.emplace_back(it->path());
		if (ec) return std::unexpected(path.string() + ": " + ec.message());
		return result;
	}
}
