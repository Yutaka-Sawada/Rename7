# Rename files to 7-bit ASCII

This replaces Unicode to %XXXX in filenames under current directory.

Options:  
/e or /encode  : rename filenames by replacing Unicode to %XXXX  
/d or /decode  : restore original filenames with Unicode  
/y or /yes     : rename automatically  
/r or /recurse : search sub-directories recursively  
/s or /space   : substitute each space to %0020  
/f or /force   : substitute each valid %XXXX to %0025XXXX  
/l or /locale  : replace only invalid characters at the locale  
/i or /invalid : replace only invalid filenames on Windows OS  
/u or /unicode : try to display Unicode characters  


### [ Usage ]

&nbsp; This application renames files and/or folders, which include non-ASCII characters.
1 Unicode character becomes 5 ACSII characters (%XXXX).
All files under the current directory will become target.

&nbsp; Option starts with "/" or "-". So, "/yes" and "-yes" are same.
They are case-insensitive. So, "/yes" and "/YES" are same.
You may use the first character of option. So, "/e" and "/encode" are same.
You may set multiple options. When options are exclusive, later one is prior.

&nbsp; To see list of options, start "Rename7.exe" without "/e" nor "/d" options.
It shows how many files will be renamed, too.
If an error will happen, you can check before actual renaming.


### [ About "/yes" option ]

&nbsp; If you are lazy to type 'Y' or 'Yes' at every renaming, set an option "/yes".
When you are annoyed with typing 'y' at every following files, type 'A' or 'All'.
If you want to skip one filename, but want to continue next, type 'N' or 'No'.
If you want to abort renaming all following files, type 'Q' or 'Quit'.


### [ About "/recurse" option ]

&nbsp; If you want to rename folders' name in addition to files's name 
under the current directory, set "/recurse" option.
It will search each sub-directory recursively, and rename all files and/or folders.


### [ About "/space" option ]

&nbsp; If you want to erase each space in a filename, you may set an option "/space".
Then, one " " is replaced to "%0020".
When there are multiple spaces, "  " is replaced to "%0020%0020".


### [ About "/force" option ]

&nbsp; Replacing process uses % as a special mark.
When there is a valid format of %XXXX already in a filename,
it will fail to restore the original filename.

&nbsp; For example, a filename is "123%0020456.txt".  
Without "/force" option;  
"abc%0020xyz.txt" -> encode -> "abc%0020xyz.txt" (% is ignored)  
"abc%0020xyz.txt" -> decode -> "abc xyz.txt" (failed to restore)

&nbsp; If a filename includes %XXXX already, set "/force" option.
With the option, each valid %XXXX is replaced to %0025XXXX.
Then, it will be able to restore the original filename correctly.

With "/force" option;  
"abc%0020xyz.txt" -> encode -> "abc%00250020xyz.txt" (% is replaced to %0025)  
"abc%00250020xyz.txt" -> decode -> "123%0020xyz.txt" (restored ok)


### [ About "/locale" option ]

&nbsp; You may want to keep some non-ASCII characters, which are valid in your locale.
You can set an option "/locale" to exclude valid characters at the Console Code Page.
You can see the Code Page on Command Prompt's property window.
Be careful to use this option.
A filename of different Code Page will include other invalid characters.
Set the "/locale" option, only when you use files in a same Code Page always.


### [ About "/invalid" option ]

&nbsp; You may want to sanitize invalid filenames on Windows OS.
You cannot treat such filenames on Windows Explorer normally.
Then, you can replace them to safe names.
When you set "/invalid" option, it doesn't rename Unicode characters.
If you want to replace Unicode characters also,
you need to run Rename.exe with "/encode" option again.

&nbsp; It will sanitize filenames of below examples;  
Period at the last character  
Space at the first or last character  
Reserved words: CON, PRN, AUX, NUL, COM1..COM9, LPT1..LPT9  
Reserved words with extension


### [ About "/unicode" option ]

&nbsp; You may want to see original non-ASCII characters on renaming dialog.
When a non-ASCII character is invalid at the Console Code Page,
it's shown as ? mark on Command Prompt normally.
By using UTF-8 and proper font, you may see such character correctly.
This option changes output to UTF-8 to show Unicode characters.

&nbsp; Caution, the result depends on Windows OS version and user setting.
Most unicode characters are OK on Windows 10.
But, you need to change Command Prompt's font on Windows 7.


### [ How to use ]

1) Open Command prompt of Windows OS.

2) To change current directory to your target, type like below;
CD "path of the directory"

3) To see number of possible files under current directory, type like below;
"path of the .EXE file\Rename7.exe"

4) To rename files under current directory, type like below;
"path of the .EXE file\Rename7.exe" /e

5) To restore renamed files under current directory, type like below;
"path of the .EXE file\Rename7.exe" /d
