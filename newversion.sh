#!/bin/bash

if [ ! -z $1 ]; then
	#sed -i -e "s/currentVersionTxt = \"[^\"]*\";/currentVersionTxt = \"$1\";/" mainwindow.cpp ;

	virgule=`echo -n $1 | sed -e 's/\./,/g'`
	virguleespace=`echo -n $1 | sed -e 's/\./, /g'`
	sed -i -e "s/FILEVERSION .*\$/FILEVERSION $virgule,0/" rddzscraper.rc ;
	sed -i -e "s/PRODUCTVERSION .*\$/PRODUCTVERSION $virgule,0/" rddzscraper.rc ;
        sed -i -e "s/VALUE \"FileVersion\", \"[^\"]*\"/VALUE \"FileVersion\", \"$virguleespace, 0\"/" rddzscraper.rc ;
        sed -i -e "s/VALUE \"ProductVersion\", \"[^\"]*\"/VALUE \"ProductVersion\", \"$virguleespace, 0\"/" rddzscraper.rc ;
else
	echo "Usage: $0 <version>";
fi
