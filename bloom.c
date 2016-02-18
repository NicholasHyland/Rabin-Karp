
/***********************************************************
 File Name: bloom.c
 Description: implementation of bloom filter goes here 
 **********************************************************/

#include "bloom.h"

/* Constants for bloom filter implementation */
const int H1PRIME = 4189793;
const int H2PRIME = 3296731;
const int BLOOM_HASH_NUM = 10;

/* The hash function used by the bloom filter */
int
hash_i(int i, /* which of the BLOOM_HASH_NUM hashes to use */ 
       long long x /* the element (a RK value) to be hashed */)
{
	return ((x % H1PRIME) + i*(x % H2PRIME) + 1 + i*i);
}

/* Initialize a bloom filter by allocating a character array that can pack bsz bits.
   Furthermore, clear all bits for the allocated character array. 
   Each char should contain 8 bits of the array.
   Hint:  use the malloc and memset library function 
	 Return value is the allocated character array.*/
bloom_filter
bloom_init(int bsz /* size of bitmap to allocate in bits*/ )
{
	bloom_filter f;

	assert((bsz % 8) == 0);
	f.bsz = bsz;
	// change memory size to fit to bytes by dividing by 8
	int size = (bsz / 8); //alternative can be (bsz >> 3) because 3 spots in binary (+ 1) is equal to 8 (but we already assert that bsz % 8 == 0)
	f.buf = malloc(size*sizeof(char)); //allocate the 8 bits to char array
	memset(f.buf, 0, size); //set 0 to clear all bits
	return f;
}

/* Add elm into the given bloom filter 
 * We use a specific bit-ordering convention for the bloom filter implemention.
   Specifically, you are expected to use the character array in big-endian format. As an example,
   To set the 9-th bit of the bitmap to be "1", you should set the left-most
   bit of the second character in the character array to be "1".
*/
void
bloom_add(bloom_filter f,
	long long elm /* the element to be added (a RK hash value) */)
{
	int x;
	int bloom;
	for (x = 0; x < BLOOM_HASH_NUM; x++) {
		//find bit position
		bloom = hash_i(x, elm) % f.bsz;
		//shift bit to the right by one using bitwise or function
		f.buf[bloom / 8] |= (1 << (7 - bloom % 8));
	}
	return;
}

/* Query if elm is a probably in the given bloom filter */ 
int
bloom_query(bloom_filter f,
	long long elm /* the query element (a RK hash value) */ )
{
	int x;
	int bloom;
	
	for (x = 0; x < BLOOM_HASH_NUM; x++) {
		//hash element using hash function to check state of bit
		bloom = hash_i(x, elm) % f.bsz;
		//check if result contains bits using bitwise and
		if (!((f.buf[bloom / 8]) & (1 << (7 - bloom % 8)))){
			return 0;
		}
	}	
	return 1;
}

void 
bloom_free(bloom_filter *f)
{
	free(f->buf);
	f->buf = (char *)0;
       	f->bsz = 0;
}


/* print out the first count bits in the bloom filter, 
 * where count is the given function argument */
void
bloom_print(bloom_filter f,
            int count     /* number of bits to display*/ )
{
	assert(count % 8 == 0);
	for(int i=0; i< (f.bsz>>3) && i < (count>>3); i++) {
		printf("%02x ", (unsigned char)(f.buf[i]));
	}
	printf("\n");
	return;
}
