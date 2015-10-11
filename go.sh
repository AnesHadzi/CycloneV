#! /bin/sh

insmod communication_driver.ko

./fswebcam original.jpg
./jpg2bmp original.jpg original.bmp

# Display original image
fbi -T 2 original.bmp

./process original.bmp

# Display processed image
fbi -T 2 fromnios.bmp

