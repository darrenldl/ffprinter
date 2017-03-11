# ffprinter
Manage file fingerprints in a manner similar to accessing a file system directory structure

## Update 2017-03-12
This project has been halted/terminated, at least with this C version. ffprinter may be rewritten in another language in future.

### Project timeline
  - Dec 2015 - Feb 2016 : Drafting and design phase
  - Feb 2016 - Sep 2016 : Implementation phase
  
### Summary of experience over the course of this project
While this project is a failed attempt at achieving my original goal, the work I made towards this project have given me great insight into different aspects such as designs in data structres, algorithms, best coding practices, file serialization/deserialization, memory leak avoidance, unit testing, reducing repeated code via use of code generation techniques(C macro in this particular context).

A similar project I discovered which does most of what I originally envisioned for ffprinter would be TMSU (https://github.com/oniony/TMSU), albeit with slightly different emphasis and design.

### Following is an elaboration/reflection on the aspects in which I feel I have gained reasonable insights in :

#### File serialization/deserialization
While most likely trivial in other higher level programming languages, implementation this from scratch in C exposed me to the issue of handling differences in endianness of CPU, and how that may affect one's file format design.

Additionally, my specific code files also reflected how fragile the serialization/deserialization code can be when done manually, and overall cumbersomeness of updating the code, as well as difficult maintenance due to repeated code.

Possible solution in retrospect : Code generation using another language, which is quite common for many projects

#### Memory leak avoidance
Prevention of memory leak in C has been problematic, and this has raised issues in security related projects(e.g. OpenSSL).
The major difficulty comes from the fact that C compilers often optimize data wiping code away, and compilers are inconsistent in treating "volatile" operations even when developers do use "volatile" in hope to avoid such issues.

Most obvious alternative in this regard would be using Rust or whichever language that provides explicit memory operations which are guaranteed in standard to not be optimized away.

#### Unit testing
All the test cases are written manually in this project. While they have been effective in ensuring code updates do not break certain functionalities, they have been ineffective in catching subtle bugs which often only reveal after a more involved manual testing.

Adopting a functional programming paradigm(or mainly the idea of separating code into modules which has no side effects) would have been helpful in isolating problems and pinpointing the location of error. Additionally, using a language which supports QuickCheck and various other auto testing framework would reduce errors and development time tremendously.

#### C macro limitation
Since no other macro system was used, C preprocessor was the only option in reducing code repetition. However, due to it being a simple textual replacement system, it has proven to be difficult to debug, and a lot of the times the macro is written from an already working code to allow easier debugging.

Macro systems in Racket, Rust, template system in C++, would have been helpful in avoiding repeated code.

### This is the end of the summary, below is the original project description

## Usage
Common use case : create a new database, fingerprint a file, save the database

See here for other usage examples

## Installation
Requires POSIX(mainly for time related functions).
Requires OpenSSL, GNU Readline, and Ncurses library, during compilation.
They are required during runtime if not compiled statically (current makefile is configured to compile statically).

## References/Resources

## Credits
UX/UI feedback, general feedback : [M. D. Chia](https://github.com/mdchia)

## License
GPLv3

## Security warning
  - Do not fingerprint extremely sensitive files(e.g. keyfiles), unless the database is well protected, as the fingerprints can reveal (partial) content of the files, due to small extractions of file content stored in fingerprint, or due to bruteforcing of file content using information of file checksum, file size, section checksum, and section size

## Remark after finishing the project
  - Writing this in C was not terribly brilliant. While fully functional and work as well as I originally intended, current lack of concurrency in ffprinter(C ver.) makes certain operations less efficient as they could be(multi hash computation).
  - Also, despite use of C macros as simulation of templates, a lot of code duplications were very difficult to avoid, and C++ templates would be very useful.
  - As it stands, extending ffprinter is more difficult than one would like as well(e.g. adding another hash function).
  - Finally, ffprinter(C ver.) was written with purely the terminal interface in mind(thus everything is sequential), so linking the code to any form of GUI, which would be handy to have, can be difficult. And database model used is not concurrent at all, which limits the efficiency in a GUI environment.

## Future plan
  - Testing (see question below on the safety of using ffprinter)
  - Porting to C++. ffprinter served as an educational experiment to me, which I expected to fail completely. Now that it didn't, I would like to learn C++ and practice it via porting to C++, since I have never been a great C++ programmer. Final port, when finished, will appear in this repo, under folder named src++.

## Notes to people wanting to do something similar
  - Technical design documents will be put up, and they alone should suffice in describing what's going on in ffprinter without digging through the code. I will add my remark and criticisms in the design documents which will hopefully be useful to people who are looking into similar projects of similar scale.

## Questions you may want to ask
### What can I use ffprinter for?
Some things you can do :
  - search for a fingerprinted file on your existing system
  - verify integrity of files(recursively)
  - check if a file is recorded in the database already(useful for avoiding duplicates)
  - tag files(stored in database)
  - attach message to files(stored in database)
  - attach a newer fingerprint to an older fingerprint to achieve some level of version control

### Why wouldn't I want to just use a list of checksums stored in a text file?
Some reasons:
  - no hierachical structure in the text file
  - no (convenient) interface dedicated for accessing the text file, while ffprinter's interface is designed for easy traversal and access of file fingerprints
  - no tagging, no attachment of user message, no quick searching on particular fields, no timestamps...

#### Obviously, a text file can be more convenient for myriad of purposes(portable, easier to recover, no dedicated software needed etc). So choose wisely.

### What is a fingerprint?
A full fingerprint of a non-empty file consists of mainly the file size, the checksums of the entire file, the checksums of each section of file.

The file is split into multiple sections to be fingerprinted individually to enable detection of partial corruption.

The number of sections the file is split into is determined based on the file size, whic can be overridden by specifying an alternate configuration during "fp" command.

See here for the full specification of the fingerprint.

### What does the database look like in memory?
A tree structure is used in memory for entries.

See here for the full documentation of what are present in memory.

### How does the database work?
The database is essentially a serialisation of the flattened tree structure.

See here for the full specification of the database format.

### How safe is it to use it?
In terms of data leakage in memory :
  - Significant amount of effort has been put into making sure related memory is wiped when a fingerprint is removed, a database is unloaded etc. Testing via external memory extraction/inspection tools is still in progress.
  - You can use the internal command scanmem to scan memory for specific pattern within ffprinter memory. However, it is better to use an external tool for memory inspection to confirm it.

In terms of database safety :
  - ffprinter does not encrypt the database.
  - Database file does not contain any form of checksum of itself. Use cryptographic hashes and/or a digital signature if you wish to verify integrity of it.

In terms of if ffprinter can be attacked (easily)
  - Loading of database contains significant amount of code for format checking. Fuzzing is under progress however.
  - Command prompt relies on GNU Readline library. Given there is no issue in Readline, then there should be very little risk in using the command prompt interface. Fuzzing is under progress however.
  - No special operation is done when reading files during fingerprinting process. Major library used is OpenSSL, which is used for hasing(SHA1, SHA256, SHA512). Fuzzing is under progress however.
  - ffprinter does not connect to network, the only interaction to outside world would be command prompt, reading/writing of database file, and read only access of files during fingerprinting.

#### Regardless, it is good practice to run ffprinter as a normal (and restricted) user, rather than root or account with any privilege, unless necessary.
