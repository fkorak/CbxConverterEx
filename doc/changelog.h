/** \file
	\brief Cbx converter changelog
*/

/** \page changelog Changelog

Version 0.01:
- initial release

0.02:
- added images width column to file list to help select optimal resize factor

0.03:
- RMB menu: clear all converted, delete selected
- aborting jobs on quit
- deleting existing temp directory before unpacking
- FIXED: file list flickering
- WARN: long file names (log only)
- settings: priority for worker processes

0.04:
- FIXED: image files with uppercase extension not included in file statistics
- FIXED: files with read-only attribute inside archive were not deleted resulting
in duplicate pages in output archive
- settings: play sound when done

0.05:
- added default scaling for specified picture size(s) setting; new scaling list entries can be added with down arrow

0.06:
- settings: extra parameters can be passed to cwebp.exe (e.g. -q 70)
- cwebp.exe updated to 0.4.0 (note: as previously it's slightly modified from base build, percentage
scaling option is added)

0.07:
- FIXED: problem with saving conversion thread priority (going back to default after restart)
- FIXED: with source containing png images resizing was set by default to 0% resulting in no conversion

0.08
- new resize mode, intended for converting archives with images with random/mixed sizes - resizing images to specified size
only if they are bigger (either width or height) than specified
- resize: added list with common resize percentages (50%, 33%) to choose without typing

0.08.1
- three submodes for conditional resize: resizing is higher or wider, only if wider only only if higher

0.09
- cwebp replaced with ImageMagick to allow more flexibility for input and output formats, in particular using CbxConverter only to resize and repack to cbz, keeping images in jpg format for platforms for which it may be hard to find reader with WebP support (iOS)
- added "Output extension" (by default defining compression method) to settings
- ImageMagick comes with most likely better resampling algorithms and overhelming list of options that may be even not possible to use at the moment, e.g. I don't know of comic book reader with support for FLIF
- if upgrading - add -quality 75 in settings as extra parameter to keep same compression level

0.10
- settings: added options to set custom temp and output directories
- settings: extra parameters for ImageMagick can be now split into ones placed before resize (new option) and ones placed after it (old setting)
- added "Delete source file(s)" to context menu
- added output file size and output/input ratio to list

0.11
- pdf import using GhostScript (separate download/install)

0.11.1:
- updated 7-zip files to version 18.01
- default position for settings and log windows changed from desktop center (might cause problem in multimonitor setup if second monitor is turned off) to screen center
- secondary windows can be closed with Esc

0.11.2:
- updated 7-zip files to version 18.05

0.12
- ImageMagick is now used to identify images - slower but able to show widths of all image types (including temporary png files used when rendering pdf files)
- File menu: added options to open temp and output directory

0.12.1
- accepting directories with drag and drop (reading recursively)
- added option to use source file directory for output file; to avoid file name conflict image extension is added to output file name, e.g. AAA.cbz -> AAA.webp.cbz

0.12.2
- fixed missing output file size info when using original directory for output (0.12.1)

0.13
- File menu: added "Add files from source directory (recursively)" option (and directories are still accepted when dropped into application)
- Settings/Directories: option to recreate source directory structure in output directory if directory is added as source (from File menu or dropped into application)
- directories are shown on file list if directory was added as source (e.g. Series\SerieA\TitleA.cbz)
- multithreading (used when multiple comic books are processed at once) - new setting: General/Worker thread count
- file list context menu: added option to delete ouput file(s) (e.g. deleting output files with poor compression ratio)
- added sorting function to file list, clicking on list header (e.g. sorting output files by compression ratio)
- fixed "Play sound when done" option (sound was always played)
- added warning if CbxConverter is placed inside folder with path longer than 25 characters (risk of hitting MAX_PATH limit)

0.13.0.3
- fixed File/Open tmp directory, File/Open ouput directory if custom folders are specified in configuration

0.13.0.4
- Settings/General: option to specify file mask for files (images) that should not be converted (e.g. by extension)

0.13.0.5 = 0.14
- removed hard limits for source file size (2 GB) and output file size, changed few variables to 64 bit wide
- minor adjustments for window sizes
- added file name to log if image identification failed (probably because file was actually not an image)
- splitted "Unpacking..." file state into "Unpacking..." + "Identifying..."
(identifying image sizes usually takes much longer than unpacking itself and grouping these
two states together was confusing)
- added progress percentage for "Identifying" and "Converting" states
- added "avif" and "jxl" (JPEG XL) to output format examples in settings window

0.14.0.1
- added Select All / Ctrl + A to the list

0.14.0.2
- slightly modified "resize" dialog window

0.14.0.3
- Settings: option to set password used for archive unpacking
- handle/show error when unpacking source archives (e.g. wrong password)

0.14.0.4
- FIXED: adding trailing backslash to temporary directory path (if needed)
- added option to copy directly source file to output directory on conversion error (e.g. invalid file password)

0.15
- added "Add new rule" button next to default resizing list to make it more obvious that it can be extended;
previously new entry for default resizing rules could be added with down arrow - not obvious
- updated bundled 7z to version 25.01 (2025-08-03)
- updated bundled ImageMagick to version 7.1.2.3 (ImageMagick-7.1.2-3-portable-Q8-x86.zip)
- added MSVC 2015 runtime dlls required by included ImageMagick version

0.16
- added function/window renaming files in temporary folder
	- main purpose: merging multiple comic books or their fragments - this requires consistent file naming; files should be also resized to at least similar sizes for convenience when reading
	- option to skip packing converted images into new cbz file, leaving images in temporary folder for manual merging
	- configurable pattern for generating new names (default: four digits with leading zeros)
	- option to extract page number from existing file
		- option to add positive or negative offset to extracted number
		- can be used also to fix inconsistent page numbering (without leading zeros)
	- option to start numbering files from specified value
	- images and other files can be deleted from the renaming list window
	- renaming list shows dimensions and sizes in bytes of images - making it easier to identify ads or odd files to delete
	- option to open selected image in default viewer
- default worker thread count is now set to half of the number of processors reported by GetSystemInfo()
- maximum number of worker thread increased to 64

0.20 (CbxConverterEx, modified by Fabian Korak / github.com/fkorak, developed with AI assistance: Claude Code by Anthropic)
- renamed to CbxConverterEx to mark it as a modified version; CbxConverter.ini from the same directory is imported on first start
- ported from C++Builder/VCL to Visual Studio: native C++20, WinUI 3 (Windows App SDK) user interface
	- dependencies are managed by vcpkg (images, archives) and NuGet (Windows App SDK) instead of bundled executables
- images are converted in-process (libwebp, libjpeg-turbo, libpng, stb) instead of running magick.exe for each image
	- images of all archives are converted in parallel by a shared thread pool - a single archive now uses all cores
	- image dimensions are read from file headers instead of decoding whole images with "magick identify"
	- JPEG images that are downscaled anyway are decoded at reduced size (DCT scaling)
	- grayscale images are encoded directly as WebP luma, fully opaque alpha channels are dropped
	- free-form ImageMagick parameters are replaced by output format, quality, WebP effort and lossless settings;
	  quality is migrated from "-quality N" in ImExtraParams
- archives are unpacked and packed with libarchive (zip/rar/rar5/7z/tar); 7z.exe is only used as fallback (e.g. encrypted rar)
- FIXED: worker thread setting above 16 was ignored
- FIXED: converting the same archive could start while it was still being unpacked
- FIXED: renaming pattern is no longer passed directly to sprintf
- FIXED: source files with the same name in different directories no longer share a temporary directory
- Unicode file names are supported (e.g. "Lucky★Star v04.cbr"); names inside archives that are not marked as Unicode
  are read as UTF-8 if valid (archives from Linux/macOS), otherwise in the DOS code page (older Windows tools)
- added Stop button, progress bars, natural sorting of file names, "Show output file in Explorer"
- columns of the file list can be resized by dragging the dividers in the header (double click resets a column); widths are saved
- added option to keep the original image if the converted one is not smaller
- files and directories passed on the command line are added to the list
- default worker thread count = number of logical processors
*/
