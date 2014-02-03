#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {

  if(argc != 3){
    fprintf(stderr, "\nusage : %s [filename] [var name]\n\n", argv[0]);
    exit(1);
  }

  char *fn   = argv[1];
  char *name = argv[2];
  FILE *f    = fopen(fn, "rb");
  
  printf("char %s[] = {\n   ", name);

  unsigned long int n = 0;
  unsigned char c;
  while(!feof(f)) {
    if(fread(&c, 1, 1, f) == 0) break;
    printf("0x%.2X,", (int)c);
    n++;
    if(n % 10 == 0) printf("\n   ");
  }

  fclose(f);
  
  printf("\n};\n");
  printf("unsigned long int %s_len = %ld;\n", name, n);

  exit(0);
}
