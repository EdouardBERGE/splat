# splat

Splat V1.0 (c) Stephen Cantini - portable C version

FILE/A = The file to be splitted
S=SEARCH/K = String to search for (\t tab, \n LF, \r CR, \\ backslash, \? question mark).
SIZE/N = Split at this number of bytes.
K/S = Size is expressed in kbytes.
LINES/S = Size is expressed in lines.
TO/K = Optional destination directory/filenames (DEFAULT:input file's).
R=RECURSIVE/S = Split EVERY size bytes/k/lines/search.
QUIET/S = Sshhhh!! :)
BUF=BUFFER/K/N = I/O buffer size in kbytes.
PARTS/S = Divide in SIZE parts.
NOCUT/S = Do not trunc text lines.
QUICK/S = Output JUST the number of files created.
NOCASE/S = Make string search case insensitive.
DOWILD/S = Allow jolly character "?" in search string.
MAX/K/N = Max number of files to create.
