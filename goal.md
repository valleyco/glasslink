# a wirless ESP32 display (CYD)

## Communication
- It will use mqtt for general comunication
- Consider an option to pull objects from an http API. (based on the MQTT message)
- Other protocol are an option if needed

## Display language
I think about 3 approaches with some possible flovours

* Raster (possible with partial rectangle)
* Draw commands
    * allow define groups by index for fast redraw
    * allow to get value (for text draw command for example) from MQTT
* Hi level UI structure (panels, gauges etc)

## Compression
For raster I want to try to use compression
- image compression
- image sequence compression

Though there are many compression implementetions, only few are suitable for us (memory/cou constraint)
I want to try do implement my own simplistic one and compare to other relevent existing solution

### A single image compression
Based on diff between adjusent points and lines

Stage 1
- from the last line to the 2nd line encode each line as the diff from previos
- from each line pixel from the last to 2nd, encode the pixel is diff from previous
decode works the opposite
Stage 2
use RLE to encode the result, we may add fixed huffman encoding
consider working on each plane alone
consider working on YUV