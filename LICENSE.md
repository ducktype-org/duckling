# DuckType Compiler License, Version 1.0

**Abbreviation:** DTCL-1.0

**Notice:** Copyright © 2026 DuckType LLC.

## PREAMBLE

*This Preamble is a statement of intent. It grants no rights and imposes no obligations. Where it and the Terms and Conditions differ, the Terms and Conditions control.*

This license is a **source-available** license. It is designed to be very close to an open-source license for most use, restricting only a narrow set of things described below. The Duckling programming language, its compiler, virtual machine, and related tools are intended to be licensed under **open-source licenses** in the future.

The language is still changing significantly, and during this period we want to keep the ecosystem coherent while it matures. We intend to move to an open-source license once Duckling is more stable and more widely recognized.

The main intention of the current License is as follows:

* Programs you write in Duckling, and everything compiled from them, are entirely yours.
* Anyone may use, study, test, and modify The Software for free, commercially or not.
* Any software may invoke the compiler or embed the runtime, and distributions may ship it with their usual patches.
* Proposals to us and research experiments are welcome, if their changes are published and clearly labelled.
* Other languages may target Duckling's bytecode or use the unmodified compiler and virtual machine through their public interfaces.

We restrict only a narrow set of things:

* Distributing a competing implementation: a version that changes the
  Duckling language, is adapted to compile or run another language,
  presents itself as a different language or as an alternative to our
  implementation, or is offered for production use in place of it,
  including as a hosted service.
* Distributing modified versions under a name of their own.
* Distributing modified versions, or offering them over a network, without publishing their source.

If these terms do not work for what you want to do, write to `licensing@ducktype.org` and ask.

END OF PREAMBLE

## TERMS AND CONDITIONS

### 1. Definitions

**"License", "The License"** means the terms and conditions as defined in this document.

**"The Software"** means the work We make available under this License, in any version, in the form in which We make it available. Each release, revision, and commit We publish is a version of The Software.

**"We", "Us", "Our"** means DuckType LLC and its successors in interest to the copyright in The Software.

**"You", "Your"** means the individual or legal entity exercising rights under this License.

**"Third Party"** means any person other than You and Us. Your employees, and contractors acting on Your behalf and under Your direction, are not Third Parties while they use The Software for You.

**"Modification"** means any change to The Software made by anyone other than Us, including any addition, deletion, translation, or adaptation, and any material that is built and Distributed as part of The Software rather than as a separate program. A Separate Work (Section 5.2) is not a Modification.

**"Modified Version"** means The Software with one or more Modifications.

**"Distribution Patches"** means Modifications that only fix defects or security vulnerabilities, backport fixes We have published, or adapt The Software to build, install, or run on a particular operating system, hardware architecture, or software distribution, without otherwise adding features or changing its behaviour.

**"Duckling"** means the programming language, together with its bytecode format and the behaviour of its virtual machine, defined by the Duckling documentation We publish, as in effect from time to time, and, on any matter the documentation does not address, by the behaviour of the versions of The Software We have made available.

**"Language Change"** means a change made in order to cause a Modified Version to accept, reject, or interpret programs, in source or compiled form, differently from Duckling as it stood in any version of The Software We have made available.

**"Competing Implementation"** means a Modified Version to which any one or more of the following applies: (a) it is named, described, marketed, or otherwise presented, expressly or by implication, as a programming language or platform other than Duckling, or as an alternative to, successor to, or replacement for Duckling or The Software; (b) its principal function is compiling, translating, or running programs written in a programming language or bytecode format other than Duckling's; (c) it implements a Language Change; or (d) it is offered, supported, or promoted to Third Parties for production use in place of The Software, including for remote interaction, unless its Modifications are limited to Distribution Patches.

**"Competing Modifications"** means Modifications, in any form and whether or not Distributed together with The Software, including patches, diffs, patch sets, overlays, replacement or additional source files, and programs that generate or apply them, that, if applied to The Software, would produce a Competing Implementation. For this purpose, the resulting Modified Version is treated as named, described, offered, supported, and promoted in the same way as the Modifications themselves.

**"Compiler Output"** means any artifact produced by running The Software or a Modified Version on input You supply, including object code, machine code, bytecode, assembly, intermediate representations, executables, libraries, diagnostics, debug information, and generated source code, and including Redistributable Components and other fragments The Software emits in the ordinary course of generating code from that input. Compiler Output does not include either of the following: (a) any artifact, or portion of an artifact, produced from input that consists of or is derived from The Software or a Modified Version, other than from Redistributable Components; or (b) any portion of an artifact that reproduces source code of The Software, other than a Redistributable Component, in whole or in substantial part, in a form that serves as source code of The Software, however that portion comes to be present.

**"Redistributable Component"** means material We supply as part of The Software that is designed to be incorporated into Compiler Output, including the standard library, runtime library, startup code, headers, and intrinsics, as listed in the file REDISTRIBUTABLE in each version. If that file is absent from a version, the standard library and runtime library of that version are Redistributable Components.

**"Corresponding Source"** means the complete, unobfuscated source code of a work in the preferred form for making modifications to it, together with the scripts, configuration, and build instructions needed to build it.

**"Distribute"** means to make a copy available to a Third Party by any means, whether or not for a fee. For the purposes of Sections 3 and 4, making a Modified Version available to Third Parties for remote interaction, such as over a network, is also Distributing it.

**"The Marks"** means Our names, trademarks, service marks, and logos, including "DuckType" and "Duckling".

### 2. License Grant

Subject to this License, We grant You a worldwide, irrevocable, royalty-free, non-exclusive, non-transferable, perpetual license under Our copyrights to use, copy, run, study, test, benchmark, modify, publicly perform, publicly display, and Distribute The Software and Modified Versions, for any purpose other than a Restricted Act (Section 4), subject to the conditions in Section 3.

Each recipient of The Software receives this license directly from Us. All rights not expressly granted are reserved. We may also make The Software available under other terms at any time, including under a license approved by the Open Source Initiative, without affecting the rights granted under this License for any version We have made available under it.

By exercising any right granted by this License, You accept its terms,
including Section 4, as binding on You.

### 3. Conditions on Distribution

**3.1 Notices.** Every copy of The Software or of a Modified Version that You Distribute must include a complete, unaltered copy of this License, retain all copyright, patent, trademark, and attribution notices contained in The Software, identify a publicly accessible location of its Corresponding Source, and, if it is a Modified Version, state that it has been modified.

**3.2 Source of Modifications.** If You Distribute a Modified Version, You must publish the Corresponding Source of all Modifications it contains, publicly and at no charge, under this License, no later than the date You first Distribute it, and keep it available for as long as You Distribute it.

**3.3 Identification.** A Modified Version that You Distribute for use as a compiler, runtime, or toolchain must be identified only by the name We use for The Software, together with a clear indication that it has been modified and by whom. If its Modifications are limited to Distribution Patches, a version identifier showing that it has been patched, such as a distribution-specific suffix, is a sufficient indication. In either case, it must not be presented as Your own compiler, runtime, or toolchain, or as supplied, endorsed, or supported by Us.

### 4. Restricted Acts

The following acts ("Restricted Acts") are not licensed, except as Section 5 expressly permits:

**4.1 Competing Implementations.** Distributing a Competing Implementation.

**4.2 Competing Modifications.** Distributing Competing Modifications.

### 5. Additional Permissions

**5.1 Unmodified copies.** You may Distribute unmodified copies of The Software, alone or as part of any product, collection, or image, including operating-system distributions, package registries, container and virtual-machine images, continuous-integration images, editors, IDEs, SDKs, and hardware, whether or not for a fee, provided that Section 3 is met and You do not present The Software as Your own compiler or toolchain. Configuration files, packaging manifests, wrapper scripts, and build recipes that invoke The Software without altering it are not Modifications.

**5.2 Separate Works.** You may invoke The Software as a separate process, including over a network or as a language server, and You may link, embed, or load unmodified The Software, including its compiler as a library and its runtime or virtual machine within an application, through the interfaces it exposes. A work that does so (a "Separate Work") is not a Modification or a Modified Version, and You may Distribute it under any terms, provided that each copy of The Software it includes is unmodified and Section 3 is met for that copy. The same applies to an application that embeds a Modified Version of the runtime or virtual machine, not offered for use as a separate compiler, runtime, or toolchain, even if that Modified Version would otherwise be a Competing Implementation under clause (d) of the definition of Competing Implementation, provided that Sections 3 is met for that Modified Version.

**5.3 Proposals and research.** You may Distribute a Modified Version that would otherwise be a Competing Implementation, or Competing Modifications, if You do so for either of the following purposes: (a) proposing changes to Us, including by publishing a fork or branch of Our source code repository; or (b) academic research, teaching, or non-commercial experimentation. In either case, all of the following conditions must be met: (i) Section 3 is met; (ii) the Modified Version is Distributed at no charge; (iii) it is not offered, supported, or promoted for production use or as part of a commercial product or service; and (iv) it is clearly labelled as an experimental variant that is not part of Duckling.

**5.4 Study and publication.** You may reverse engineer, decompile, test, fuzz, audit, benchmark, and analyse The Software, and publish the results, including comparative results and results unfavourable to Us, without notifying Us or seeking Our approval.

### 6. Compiler Output

**6.1 No rights asserted, no conditions.** We claim no copyright, patent, or other right in Compiler Output, whether produced by The Software or by a Modified Version. You may use, modify, Distribute, sell, and license Compiler Output for any purpose and on any terms, including proprietary terms, without attribution, royalty, or any obligation under this License. No condition of this License applies to Compiler Output or to any program, product, or service that includes it.

**6.2 Redistributable Components.** Section 6.1 covers a Redistributable Component only as contained in Compiler Output. A Redistributable Component that You Distribute on its own, as source code or as a separately identifiable component, is governed by this License, including Section 5.2.

**6.3 Third-party material.** Sections 6.1 and 7.1 concern only rights held by Us and by persons who license their Modifications under this License. Material in Compiler Output owned by anyone else, including material in Your input, remains subject to its owner's terms.

### 7. Patents

**7.1 Grant.** Subject to this License, We grant You a worldwide, royalty-free, non-exclusive, perpetual license under the patent claims We own or control that are necessarily infringed by The Software, to make, use, sell, offer for sale, import, and otherwise exploit The Software for any purpose this License permits. We also grant You an irrevocable license under those claims to make, use, sell, offer for sale, import, and otherwise exploit Compiler Output without limitation.

**7.2 Defensive termination.** If You start or voluntarily join patent litigation or other patent enforcement against Us or any recipient of The Software, alleging that The Software or a contribution to it infringes a patent, Your license under the first sentence of Section 7.1 ends on the date that action starts. Your license for Compiler Output does not end.

**7.3 Scope.** The license in Section 7.1 does not extend to claims infringed only by a Modification made by someone other than Us, or only by a combination of The Software with material We did not supply. Running The Software on any hardware or operating system, and processing any input with it, are not such combinations.

### 8. Trademarks

**8.1** This License grants no rights in The Marks except as this Section states.

**8.2** Without asking Us, You may use The Marks for any of the following purposes: (a) to describe the origin of The Software; (b) to state truthfully that a product is compatible with, written in, or built using Duckling or The Software; (c) to name libraries, packages, and programs written in Duckling, in a way that does not suggest they are made or endorsed by Us; (d) to comply with Section 3.1; or (e) as Section 3.3 requires.

**8.3** You may not otherwise use The Marks in the name or branding of a Modified Version, product, or service, or in any way that suggests that We endorse, sponsor, or are affiliated with You or Your product or service.

### 9. Disclaimer of Warranty

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NONINFRINGEMENT.

WE DO NOT WARRANT THAT THE SOFTWARE IS FREE OF DEFECTS OR WILL OPERATE WITHOUT INTERRUPTION OR ERROR, OR THAT COMPILER OUTPUT WILL BE CORRECT, COMPLETE, EFFICIENT, SECURE, OR SEMANTICALLY EQUIVALENT TO ITS INPUT. YOU BEAR THE ENTIRE RISK OF USING THE SOFTWARE AND OF RELYING ON COMPILER OUTPUT.

### 10. Limitation of Liability

In no event and under no legal theory, whether in tort (including negligence), contract, or otherwise, unless required by applicable law (such as for deliberate and grossly negligent acts) or agreed to in writing, shall We or any contributor be liable to You for any damages of any character, including direct, indirect, special, incidental, or consequential damages, arising from this License or from the use of or inability to use The Software or Compiler Output, including damages arising from miscompilation, even if advised of the possibility of such damages.

### 11. General

**11.1 Severability.** If any provision of this License is held unenforceable, it will be modified to the minimum extent necessary to make it enforceable or, if it cannot be so modified, severed, and the remaining provisions continue in effect. If Section 4 or any part of it is held unenforceable, the license in Section 2 is to be construed as granted only for purposes that are not Restricted Acts, and not as broader than Section 2 provides.

**11.2 No waiver.** Our failure to enforce any provision is not a waiver of it.

END OF TERMS AND CONDITIONS