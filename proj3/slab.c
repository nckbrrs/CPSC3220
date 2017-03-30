/*Alex Ward, Nick Barrs
March 29, 2017
Project 3
*/
#include "slab.h"

unsigned char *slab_allocate(){
	
	if(full_mask == 0xffff) return NULL;
	
        int slab_number;
        int i;
	for(i = 0; i < 16; i++){
		if(((full_mask >> (15-i)) & 1) == 0){
			slab_number = i;
			i = 16;
		}
		else 
			slab_number = 73;
		}	

	int object_number;
	int j;
	for(j = 1; j < 16; j++){
		if(((s[slab_number].free_mask >> (15-j)) & 1) == 1){
			object_number = j;
			j = 16;
		}
		else
			object_number = 73;
		}


	s[slab_number].free_count--;
        s[slab_number].free_mask -= (1 << (15 - object_number));

	if(s[slab_number].free_count == 14){
		empty_mask -= (1 << (15-slab_number));		
		partial_mask += (1 << (15-slab_number));
	}
	else if(s[slab_number].free_count == 0){
		partial_mask -= (1 << (15-slab_number));	
		full_mask += (1 << (15 - slab_number));
	}

	return start + (slab_number * 0x1000) + (object_number * 0x0100);
}

int slab_release( unsigned char *addr ){
	if(addr < start || addr > (start + 0xff00)) return 1;

	int slab_number = (addr - start) >> 12;
	if(s[slab_number].signature != 0x51ab51ab) return 1;

	int object_number = ((addr - start) >> 8) & 0x000f;
	int freeMaskBit = ((s[slab_number].free_mask) >> (15 - object_number)) & 1;
	
	if(freeMaskBit == 1) return 2;

	s[slab_number].free_count++;
        s[slab_number].free_mask += (1 << (15 - object_number));

	if(s[slab_number].free_count == 15){
		empty_mask += (1 << (15-slab_number));
		partial_mask -= (1 << (15-slab_number));
	}
	else if(s[slab_number].free_count == 1){
		partial_mask += (1 << (15-slab_number));
		full_mask -= (1 << (15-slab_number));
	}

	return 0;
}

