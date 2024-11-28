rem Makes a video by joining together all the screenshots found in the current directory
rem The first image is called title.png and will be showed for some seconds before starting the animation
rem ImageMagick must be installed and included in PATH. Please check the paths in this file before trying 
rem 
rem Invocatione: make_video <output.mpg>  or
rem				 make_video <output.mp4>

rem deletes previous videos
del *.mpg
del *.mp4

rem Rezizes all the images to the same dimensions, including the title.png
mogrify -scale 1024x768 *.png


rem creates the video. 
rem Convert.exe is part of ImageMagick - you have to use the full path because there is already a convert.exe in the c:\Windows folder

rem this is for 32bit Imagemagick
rem c:\"Program Files"\ImageMagick-7.0.7-Q8\magick convert -limit memory 1024 -verbose -compress None  -delay 300 title.png  -delay 10 -compress None  autoshot*.png -loop 1 %1


rem this is for 16bit Imagemagick (requires less resources)
c:\"Program Files (x86)"\ImageMagick-6.7.5-Q16\convert -debug cache -limit memory 1024 -verbose -delay 1000 title.png  -delay 15 autoshot*.png -loop 1 %1

