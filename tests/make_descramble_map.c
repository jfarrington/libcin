#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

int descramble_image(uint16_t *image_out, uint16_t *image_in,
                     uint64_t image_size_pixels, uint64_t image_height, uint64_t image_width);

int main(void){
  uint16_t *in;
  uint16_t *out;
  uint32_t *store;
  uint32_t i,j;
  uint64_t height = 964, width = 1152, pixels; 

  int forward = 0;

  FILE *fp;

  pixels = height * width;

  in = malloc(sizeof(uint16_t) * pixels);
  out = malloc(sizeof(uint16_t) * pixels);
  store = malloc(sizeof(uint32_t) * pixels);
  memset(store, 0, sizeof(uint32_t) * pixels);

  for(i = 0;i < pixels; i++){
    memset(in, 0, sizeof(uint16_t) * pixels);
    memset(out, 0, sizeof(uint16_t) * pixels);

    in[i] = 1;  

    descramble_image(out, in, pixels, height, width);

    for(j = 0;j<pixels; j++){
      if(out[j] == 1){
        if(forward){
          store[i] = j;
        } else {
          store[i] = i;
        }
        fprintf(stderr, "%ld,%ld\n", (long int)i,(long int)j);
        continue;
      }
    }
  }

  if(forward){
    fp = fopen("descramble_mapi_forward.bin", "w");
  } else {
    fp = fopen("descramble_map_reverse.bin", "w");
  }
  fwrite(store, sizeof(uint32_t), pixels, fp);
  fclose(fp);

  return(0);
}

int descramble_image(uint16_t *image_out, uint16_t *image_in, 
                     uint64_t image_size_pixels, uint64_t image_height, uint64_t image_width){


    if(!image_out || !image_in) return(0);

    uint64_t inOffset;
    uint64_t outOffset;

    //image_size_pixels is the total number of pixels to get row and columns information
    //one needs at least one of them.

    // We need to get the terminology of channels vs. columns strightened out
    // Total number of subcolumns = (Total number of fcrics on one side) x
    // (Total number of columns in fcric) x (number of real and virtual pixels in a column)
    // Canonical Example: 6 x 16 x 12 = 1152

    uint64_t RealColumnPos=0,RealRowPos=0,ImageColumPos=0,ImageRowPos=0,posInBlock=0,posInMux=0,posInCol=0;

    //---Commenting out these variable because they are unused----------
    //uint64_t outRow=0,outCol=0;
    //uint64_t output = 0;

    uint64_t eof_word_cnt = 0; // end of fcric word count: i.e. number of times we see '0xFIFO' in the byte stream
    uint64_t start_eof_counter = 0;
    uint64_t quad = 0; //Where is each quadrant located in the tiff? It would help to have a visual sense.

    uint64_t m, n;

    if (image_height*image_width == image_size_pixels){

        //if image size is correct process image and de-scramble it
        // jtw - missing a row if I have "num_row - 1".  Not sure why??
        for (m = 0; m < image_height; m++ ){

            for(n = 0; n < image_width - 1; n++ ){

                // pixel index
                uint64_t index = n + (m*image_width);
                /*
                 index range:
                 m = 0,                  n = 0                 => index = 0
                 m = (image height - 1), n = (image_width - 2) => index = [image_width - 2 + (image_height - 1)(image_width)]  = image_size_pixels - 2
                */

               //-------This is what the following code currently does.
               /*
                For eof_word_cnt = 0:

                    If index = 0,1,2,3,     16,17,18,19, 32,33,34,35,...etc. quad = 0
                    If index = 4,5,6,7,     20,21,22,23, 36,37,38,39,...etc. quad = 1
                    If index = 8,9,10,11,   24,25,26,27, 40,41,42,43,...etc. quad = 2
                    If index = 12,13,14,15, 28,29,30,31, 44,45,46,47,...etc. quad = 3

                For eof_word_cnt = 1:
                    If index = 0                                             quad = 3

                    If index = 1,2,3,4,     17,18,19,20, 33,34,35,36,...etc. quad = 0
                    If index = 5,6,7,8,     21,22,23,24, 37,38,39,40,...etc. quad = 1
                    If index = 9,10,11,12,  25,26,27,28, 41,42,43,44,...etc. quad = 2
                    If index = 13,14,15,16, 29,30,31,32, 45,46,47,48,...etc. quad = 3

                For eof_word_cnt = 2:
                    If index = 0,1                                           quad = 3

                    If index = 2,3,4,5,     18,19,20,21, 34,35,36,37,...etc. quad = 0
                    If index = 6,7,8,9,     22,23,24,25, 38,39,40,41,...etc. quad = 1
                    If index = 10,11,12,13, 26,27,28,29, 42,43,44,45,...etc. quad = 2
                    If index = 14,15,16,17, 30,31,32,33, 46,47,48,49,...etc. quad = 3

                For eof_word_cnt = 3:
                    If index = 0,1,2                                         quad = 3

                    If index = 3,4,5,6,     19,20,21,22, 35,36,37,38,...etc. quad = 0
                    If index = 7,8,9,10,    23,24,25,26, 39,40,41,42,...etc. quad = 1
                    If index = 11,12,13,14, 27,28,29,30, 43,44,45,46,...etc. quad = 2
                    If index = 15,16,17,18, 31,32,33,34, 47,48,49,50,...etc. quad = 3

                For eof_word_cnt = 4+:
                    If index = 0,1,2,3                                       quad = 3

                    If index = 4,5,6,7,     20,21,22,23, 36,37,38,39,...etc. quad = 0
                    If index = 8,9,10,11,   24,25,26,27, 40,41,42,43,...etc. quad = 1
                    If index = 12,13,14,15, 28,29,30,31, 44,45,46,47,...etc. quad = 2
                    If index = 16,17,18,19, 32,33,34,35, 48,49,50,51,...etc. quad = 3
               */

                //----------Removing this case, because it is redundant---------------
                /*if ( eof_word_cnt == 0)
                {
                    quad = (uint64_t)((index%16)/4);
                }
                else
                */

                if (eof_word_cnt < 4) //If eof_word_count 0,1,2,3
                    // WHY are we casting a fraction to an INT to truncate? There has to be a better way.
                {
                    quad = (uint64_t)(( (index - eof_word_cnt)%16)/4); //How do we know that index > eof_word_count and we are not implicitly ?
                }
                else //If eof_word_count >=4
                {
                    quad = (uint64_t)(( (index - 4)%16)/4); //If eof_word_cnt > 4, pretend it is 4?
                }



                if(quad==0){
                    /*
                    for index = 0: RealColumPos = ((3-0)*144) + (0*12) + (11-0) = 443
                    What is the block and what is the mutex? The example numbers I tried in
                    this code are not giving any hints as to the abstraction.
                    */
                    RealColumnPos = ((3-posInBlock)*144) + (posInMux*12) + (11 - posInCol);

                    //----------Again, removing this case, because it is redundant---------------
                    /*
                    if ( eof_word_cnt == 0)
                    {
                        RealRowPos = (uint64_t)(index/(2*image_width));
                    }
                    else
                    */
                    if (eof_word_cnt < 4) //If eof_word_count 0,1,2,3
                    {
                        RealRowPos = (uint64_t)( (index - eof_word_cnt) / (2*image_width));
                    }
                    else //If eof_word_count >=4
                    {
                        RealRowPos = (uint64_t)( (index - 4) / (2*image_width));
                    }

                }

                // swap first and last column and move all others back of one position
                if(RealColumnPos%12 == 0){ //If RealColumPos = 0, 12, 24, 36, etc...
                    RealColumnPos = RealColumnPos + 11;
                }else{
                   RealColumnPos = RealColumnPos - 1;
                }

                ImageColumPos = RealColumnPos;
                ImageRowPos = RealRowPos;

                if( quad == 0){ //Why quad==0 here? It looks like you want quad 1,2,3 further down....
                    //performs the memory copy

                    // quad = 0
                    // 5-08-2013 jtw add -1 to fix pixel shift on top half
                    // 5-08-2013 jtw - need to allow zero to get first pixel to be copied into image_out
                    inOffset = index;
                    //How do we know that ((ImageRowPos*image_width) + image_width) > (1+ ImageColumPos)?
                    outOffset =  (ImageRowPos*image_width) + image_width - 1 - ImageColumPos;

                    // jtw - look for 0xF1F0 instead of 0xf0f1, because the bytes get swapped later in imageprocessor.cpp
                    // do not write the 4 eof words when you get 0xf1f0
                    if ( *(image_in + inOffset) == 0xF1F0 )
                    {
                        start_eof_counter = 1;
                    }

                    if (start_eof_counter == 0)
                    {
                        // inOffset >= 0 && outOffset >= 0 has to be true because they are unsigned ints!
                        // If they are not supposed to be uints, we need to STOP implicit type casting.
                         if(inOffset < image_size_pixels && outOffset < image_size_pixels)
                            *(image_out + outOffset) = *(image_in + inOffset);
                    }
                    else if ( (start_eof_counter == 1) && (eof_word_cnt < 4) )    // do not transfer 4 words, throw away f0f1 f2de adf0 0d01.  Last 4 words??
                    {
                        eof_word_cnt += 1;
                    }
                    else if ( (start_eof_counter == 1) && (eof_word_cnt == 4) )    // do not transfer 4 words, throw away f0f1 f2de adf0 0d01.  Last 4 words??
                    {
                        eof_word_cnt += 1;
                        if(inOffset < image_size_pixels && outOffset < image_size_pixels)
                            *(image_out + outOffset) = *(image_in + inOffset);
                    }
                    else if ( (start_eof_counter == 1) && (eof_word_cnt > 4) )
                    {
                        if(inOffset < image_size_pixels && outOffset < image_size_pixels)
                            *(image_out + outOffset) = *(image_in + inOffset);
                    }

                    // quad = 1, but this can never happen. we are inside quad == 0.
                    // 5-08-2013 jtw add -1 to fix pixel shift on top half
                    inOffset = index + 4;
                    outOffset =  (ImageRowPos*image_width) + image_width/2 - 1 - ImageColumPos;

                    if(inOffset < image_size_pixels && outOffset < image_size_pixels)
                    {
                        *(image_out + outOffset) = *(image_in + inOffset);
                    }

                    // quad = 2
                    // 5-08-2013 jtw add -1 to fix missing row on bottom
                    inOffset = index + 8;
                    outOffset =  ((image_height - 1 - ImageRowPos )*image_width) + ImageColumPos;

                    if(inOffset < image_size_pixels && outOffset < image_size_pixels)
                    {
                        *(image_out + outOffset) = *(image_in + inOffset);
                    }


                    // quad = 3
                    inOffset = index + 12;
                    outOffset =  ((image_height - 1 - ImageRowPos )*image_width) + ImageColumPos + image_width/2;

                    if(inOffset < image_size_pixels + 4 && outOffset < image_size_pixels)
                    {
                        *(image_out + outOffset) = *(image_in + inOffset);
                    }


                    if ( (eof_word_cnt < 1) || (eof_word_cnt > 4) )     // do not move output pointer during 4 eof words
                    {
                        if (posInBlock == 3)
                        {
                            posInBlock = 0;
                            if (posInMux == 11)
                            {
                                posInMux = 0;
                                if (posInCol == 11) posInCol = 0;
                                else                posInCol = posInCol + 1;

                            }
                            else posInMux = posInMux + 1;
                        }
                        else posInBlock = posInBlock + 1;
                    }
                }   // quad = 0
            }       // n or colunm
        }           // m or row

    }

    return(1);
}

