import sys
import tempfile
import click


@click.command()
def write_colored_to_tempfile():
    # Create a temporary file
    with tempfile.NamedTemporaryFile(mode="w+", delete=False) as temp_file:
        # Style the text using click.style
        colored_message = click.style(
            "This is a colored message written by click.echo.",
            fg="blue",
            bg="white",
            bold=True,
        )

        # Write the styled text to the temporary file
        click.echo(colored_message, file=temp_file, color=True)

        # Move the file pointer to the beginning
        temp_file.seek(0)

        # Read and print the content of the temporary file
        print("Content of the temporary file:")
        sys.stdout.write(temp_file.read())


if __name__ == "__main__":
    write_colored_to_tempfile()
