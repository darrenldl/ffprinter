# ffprinter
Manage file fingerprints in a manner similar to accessing a file system directory structure

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
