# HeapAllocator
My program allows us to allocate memory just like allocators from [standard library](https://pl.wikibooks.org/wiki/C/Biblioteka_standardowa/Indeks_tematyczny#stdlib.h).

## Additional files
To make running my program as easy as it can get I added two files: [memmanager.c](memmanager.c) and [custom_unistd.h](custom_unistd.h). They are not my files. They are in posession of [my university](https://p.lodz.pl/). 

## Difference
To make my program run correctly we need to setup the heap with heap_setup(). After that everything runs as it should. After all that "noise" we need to clean the heap with "heap_clean()". 

## Documentation
Other functions have similar use cases as original ones [documentation](https://pl.wikibooks.org/wiki/C/malloc).

## Instructions
First we need to compile:
```
gcc -o [any_name] memmanager.c custom_unistd.h heap.c heap.h main.c
```
And to execute it:
```
./[any_name] "Sample text"
```
Expected output:
```
malloc: Sample text
calloc: Sample text
realloc: Sample text

### Stan płotków przestrzeni sterty:
    Płotek początku ....: [poprawny]
    Płotek w pozycji brk: [poprawny]
    Płotek końca........: [poprawny]
### Podsumowanie: 
    Całkowita przestrzeni dostępnej pamięci: 67108864 bajtów
    Pamięć zarezerwowana przez sbrk() .....: 0 bajtów
Naciśnij ENTER...
```
