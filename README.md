# pfspd
PFSPD (Philips File Standard for Pictoral Data) is a file format for the storage of uncompressed video sequences with arbitrary word width, color space and number of components.

# Introduction
PFSPD = Philips File Standard for Pictorial Data
The routines in this file provide an interface to pfspd video files.

The routines in this file provide the following functionality:

- Reads/writes:
    - progressive video files
    - interlaced video files
    - access per field and/or frame
- Reads/writes:
    - luminance only
    - 4:4:4 YUV (planar UV)
    - 4:2:2 YUV (multiplexed or planar UV)
    - 4:2:0 YUV (multiplexed or planar UV)
    - 4:4:4 RGB
    - streaming files ("S" files, e.g. cvbs encoded files)
- File formats supported:
    - 8, 10, 12, 14 & 16 bit per pixel files (pfspd types: B*8, B*10, B*12, B*14 & I*2).
- Memory formats supported:
    - 8 bits (unsigned char, with 8 bits data)
    - 16 bits (unsigned short, with 8, 10, 12, 14 or 16 bits data)
- Extra components with application defined name, independent file format and subsample factors.
- Easy header definition and modification for Standard Definition and several High Definition formats (including all formats defined by ATSC)
# Coding rules used in this library
General prefix for cpfspd library is p.
- all functions start with p_ followed by lower case name.
- all typedefs start with pT_ followed by lower case name.
- all defines start with P_ followed by upper case name.