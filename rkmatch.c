/* Match every k-character snippet of the query_doc document
	 among a collection of documents doc1, doc2, ....

	 ./rkmatch snippet_size query_doc doc1 [doc2...]

*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <strings.h>
#include <assert.h>
#include <time.h>
#include <ctype.h>

#include "bloom.h"

enum algotype { EXACT=0, SIMPLE, RK, RKBATCH};

/* a large prime for RK hash (BIG_PRIME*256 does not overflow)*/
long long BIG_PRIME = 5003943032159437; 

/* constants used for printing debug information */
const int PRINT_RK_HASH = 5;
const int PRINT_BLOOM_BITS = 160;


/* calculate modulo addition, i.e. (a+b) % BIG_PRIME */
long long
madd(long long a, long long b)
{
	assert(a >= 0 && a < BIG_PRIME && b >= 0 && b < BIG_PRIME);
	return ((a+b)>BIG_PRIME?(a+b-BIG_PRIME):(a+b));
}

/* calculate modulo substraction, i.e. (a-b) % BIG_PRIME*/
long long
mdel(long long a, long long b)
{
	assert(a >= 0 && a < BIG_PRIME && b >= 0 && b < BIG_PRIME);
	return ((a>b)?(a-b):(a+BIG_PRIME-b));
}

/* calculate modulo multiplication, i.e. (a*b) % BIG_PRIME */
long long
mmul(long long a, long long b)
{
	assert(a >= 0 && a < BIG_PRIME && b >= 0 && b < BIG_PRIME);
	/* either operand must be no larger than 256, otherwise, there is danger of overflow*/
	assert(a <= 256 || b <= 256); 
	return ((a*b) % BIG_PRIME);
}

/* read the entire content of the file 'fname' into a 
	 character array allocated by this procedure.
	 Upon return, *doc contains the address of the character array
	 *doc_len contains the length of the array
	 */
void
read_file(const char *fname, unsigned char **doc, int *doc_len) 
{

	int fd = open(fname, O_RDONLY);
	if (fd < 0) {
		perror("read_file: open ");
		exit(1);
	}

	struct stat st;
	if (fstat(fd, &st) != 0) {
		perror("read_file: fstat ");
		exit(1);
	}

	*doc = (unsigned char *)malloc(st.st_size);
	if (!(*doc)) {
		fprintf(stderr, " failed to allocate %d bytes. No memory\n", (int)st.st_size);
		exit(1);
	}

	int n = read(fd, *doc, st.st_size);
	if (n < 0) {
		perror("read_file: read ");
		exit(1);
	}
	else if (n != st.st_size) {
		fprintf(stderr,"read_file: short read!\n");
		exit(1);
	}
	
	close(fd);
	*doc_len = n;
}

/* The normalize procedure normalizes a character array of size len 
   according to the following rules:
	 1) turn all upper case letters into lower case ones
	 2) turn any white-space character into a space character and, 
	    shrink any n>1 consecutive whitespace characters to exactly 1 whitespace

	 When the procedure returns, the character array buf contains the newly 
     normalized string and the return value is the new length of the normalized string.

     hint: you may want to use C library function isupper, isspace, tolower
     do "man isupper"
*/
int
normalize(unsigned char *buf,	/* The character array contains the string to be normalized*/
	int len	    /* the size of the original character array */)
{
	char *str = (char*)malloc(len + 1);
	memset(str, 0, len + 1);
	int bufpos = 0;
	int prevspace = 0;

	for (int i = 0; i < len; i++) {
		char chr = buf[i];
		if (isspace(chr)){
			if (prevspace == 1) {
			
			}
			else {
				prevspace = 1;
				str[bufpos] = ' ';
				bufpos++;
			}
		}
		else {
			prevspace = 0;
			if (isupper(chr)){
				chr = tolower(chr);
			}
			str[bufpos] = chr;
			bufpos++;
		}
	}
	
	if (str[bufpos] == ' ') {
		bufpos--;
	}
	str[bufpos] = '\0';

	char *bufptr = str;
	if (*bufptr = ' ') {
		bufptr++;
	}

	memcpy(buf, bufptr, bufpos);
	free(str);
	return bufpos;

/*	int a = 0, b = 0;
	//check for preceeding white spaces, and ignore them
	while(isspace(buf[a])) {
		a++;
	}
	
	for (a; a < len; a++) {
		if (isupper(buf[a])){ //if its upper, change to lower and assign buf[b]
			buf[b] = tolower(buf[a]);
			b++;
		}
		else if (islower(buf[a])){ //if its lower, just assign buf[b]
			buf[b] = buf[a];
			b++;
		}
		else {
			if (isspace(buf[a]) && !(isspace(buf[a-1]))){ //if its a space and the previous isn't (happens once) assign buf[b]
				buf[b] = ' ';
				b++;
			}
			else if (isspace(buf[a]) && (isspace(buf[a-1]))){ //there are at least two spaces, dont assign and continue
				continue;
			}
		}	
	} 
//	while (isspace(buf[b-1])){
//		b--;
//	}

	while (buf[a] != '\0'){ //while we aren't at the end of the string
		if (isspace(buf[a])) {		
			buf[b] = buf[a]; //immediately add the first white space
			while (isspace(buf[a])){ //ignore the subsequent spaces
				a++;
			}
			b++;
		}
		else {
			if (isupper(buf[a])) { //if it is Upper case, convert to lower
				buf[a] = tolower(buf[a]);
			}
			buf[b] = buf[a]; // it is a letter, copy
			a++;	
			b++;
		}
	}
	
	buf[b] = '\0'; //null terminator at the end
	return b;*/
}

int
exact_match(const unsigned char *qs, int m, 
        const unsigned char *ts, int n)
{
//	if (m != n) { //if they aren't the same size, then obviously not the exact match
//		return 0;
//	}	

	if (m != n) {
		return 0;
	}	
	return (!strncmp(qs, ts, m));

/*	int i;
	for (i = 0; i < m; i++) { //loop through every element in size m
		if (qs[i] != ts[i]) { //if they aren't equal immediately return 0
			return 0;
		}
	}
	return 1;  //reached the end of for loop, meaning everything matched - return 1 */
}

/* check if a query string ps (of length k) appears 
	 in ts (of length n) as a substring 
	 If so, return 1. Else return 0
	 You may want to use the library function strncmp
	 */


int
simple_substr_match(const unsigned char *ps,	/* the query string */
	int k,/* the length of the query string */
	const unsigned char *ts,/* the document string (Y) */ 
	int n/* the length of the document Y */)
{

/*	for (int i = 0; i < n - k; i++) {
		if (exact_match(ps, k, ts + i, k) == 1) {
			return 1;
		}
	}
	return 0;*/

  	if (k > n) { //size of query string cannot be greater than size of document string
		return 0;
	}
	if (k == 0 || n == 0) { //if lengths are 0, then they are a match, cause it's matching nothing
		return 1;
	}
	int compare;
	for (int i = 0; i < n; i++){ //loop through each starting element of document string
		compare = strncmp(ps, ts + i, k); //compare next k characters from ps and ts (current position)
		if (compare == 0) {
			return 1;
		}
	}
	return 0;
}

long long getHash(long long number, long long power) {
	long long hash2 = 1;
	int y;
	for (y = 0; y < power; y++) {
		hash2 = mmul(hash2, number);
	}
	return hash2;
}

/* Check if a query string ps (of length k) appears 
in ts (of length n) as a substring using the rabin-karp algorithm
If so, return 1. Else return 0

In addition, print the hash value of ps (on one line)
as well as the first 'PRINT_RK_HASH' hash values of ts (on another line)
Example:
	$ ./rkmatch -t 2 -k 20 X Y
	4537305142160169
	1137948454218999 2816897116259975 4720517820514748 4092864945588237 3539905993503426 
	2021356707496909
	1137948454218999 2816897116259975 4720517820514748 4092864945588237 3539905993503426 
	0 chunks matched (out of 2), percentage: 0.00

Hint: Use "long long" type for the RK hash value.  Use printf("%lld", x) to print 
out x of long long type.
*/

int
abin_karp_match(const unsigned char *ps,	/* the query string */
	int k,/* the length of the query string */
	const unsigned char *ts,/* the document string (Y) */ 
	int n/* the length of the document Y */ )
{

	int base = 256;
	long long hash = getHash(base, k-1); //calculate 256^(k-1)
	int i;
	long long pHash = 0;
	long long tHash1 = 0;
	long long tHash2 = 0;
	int correct = 0;

	int x;
	for (x = 0; x < k; x++) {
		pHash = madd(mmul(ps[x], getHash(base, k-1-x)), pHash); //calculate intial pHash value
		tHash1 = madd(mmul(ts[x], getHash(base, k-1-x)), tHash1); //calculate initial tHash Value
	}
	
	printf("%lld\n", pHash); //print both pHash and tHash initial value
	printf("%lld ", tHash1);

	if (pHash == tHash1) { //check initial condition. set 'boolean' to true
		correct = 1;
	}	

	for (i = 0; i < n - k - 1; i++) {
//		if (pHash == tHash1) {
//			correct = 1;
//		}
		tHash2 = madd(mmul(mdel(tHash1, mmul(ts[i], hash)), base), ts[i+k]); //calculate rolling hash
		if (pHash == tHash2) {
			correct = 1;
		}
		if (i < 4){
			printf("%lld ", tHash2); //print 4 times (already printed once before)
		}
//		tHash2 = madd(mmul(mdel(tHash1, mmul(ts[i], hash)), base), ts[i+k]);
		tHash1 = tHash2; //update tHash1 to calculated rolling hash tHash2
	
	}
	printf("\n");
	return correct;
}


/* Allocate a bitmap of bsz bits for the bloom filter (using bloom_init) and insert 
 all m/k RK hashes of qs into the bloom filter (using bloom_add).  Compute each of the n-k+1 RK hashes 
 of ts and check if it's in the filter (using bloom_query). 
 This function returns the total number of matched chunks. 

 For testing purpose, you should print out the first PRINT_BLOOM_BITS bits 
 of bloom bitmap in hex after inserting all m/k chunks from qs.
 */
int
rabin_karp_batchmatch(int bsz, /* size of bitmap (in bits) to be used */
    int k,/* chunk length to be matched */
    const unsigned char *qs, /* query docoument (X)*/
    int m,/* query document length */ 
    const unsigned char *ts, /* to-be-matched document (Y) */
    int n /* to-be-matched document length*/)
{
/*	if (k > n) {
		return 0;
	}
	
   	bloom_filter bloom = bloom_init(bsz);
	int base = 256;
	long long hash = getHash(base, k-1);
	
	int match = 0;
	int output;
	for (int x = 0; x < m/k; x++) {
		bloom_add(bloom, rabin_hash(qs[x*k], k));
	}

	bloom_print(bloom, PRINT_BLOOM_BITS);
	long long tHash = rabin_hash(ts, k);

	for (int i = 0; i < n - k + 1; i++) {
		if (bloom_query(bloom, tHash)){
			for (int j = 0; j < m/k; j++) {
				output = strncmp(qs[j*k], ts[i], k);		
				if (output == 0) {
					match++;
					break;
				}
			}
		}
		tHash = madd(mmul(mdel(tHash, mmul(ts[i], hash)), base), ts[i+k]);
	}
	return match;*/
}


int 
main(int argc, char **argv)
{
	int k = 20; /* default match size is 20*/
	int which_algo = SIMPLE; /* default match algorithm is simple */

	
	/* Refuse to run on platform with a different size for long long*/
	assert(sizeof(long long) == 8);

	/*getopt is a C library function to parse command line options */
	int c;
	while ((c = getopt(argc, argv, "t:k:q:")) != -1) {
	       	switch (c) {
			case 't':
				/*optarg is a global variable set by getopt() 
					it now points to the text following the '-t' */
				which_algo = atoi(optarg);
				break;
			case 'k':
				k = atoi(optarg);
				break;
			case 'q':
				BIG_PRIME = atoi(optarg);
				break;
			default:
				fprintf(stderr, "Valid options are: -t <algo type> -k <match size> -q <prime modulus>\n");
				exit(1);
	       	}
       	}

	/* optind is a global variable set by getopt() 
		 it now contains the index of the first argv-element 
		 that is not an option*/
	if (argc - optind < 1) {
		printf("Usage: ./rkmatch query_doc doc\n");
		exit(1);
	}

	unsigned char *qdoc, *doc; 
	int qdoc_len, doc_len;
	/* argv[optind] contains the query_doc argument */
	read_file(argv[optind], &qdoc, &qdoc_len); 
	qdoc_len = normalize(qdoc, qdoc_len);

	/* argv[optind+1] contains the doc argument */
	read_file(argv[optind+1], &doc, &doc_len);
	doc_len = normalize(doc, doc_len);

	int num_matched;
	switch (which_algo) {
            case EXACT:
                if (exact_match(qdoc, qdoc_len, doc, doc_len)) 
                    printf("Exact match\n");
                else
                    printf("Not an exact match\n");
                break;
	   
	    case SIMPLE:
	       	/* for each chunk of qdoc (out of qdoc_len/k chunks of qdoc, 
		 * check if it appears in doc as a substring */
		num_matched = 0;
		 for (int i = 0; (i+k) <= qdoc_len; i += k) {
			 if (simple_substr_match(qdoc+i, k, doc, doc_len)) {
				 num_matched++;
			 }
			
		 }
		 printf("%d chunks matched (out of %d), percentage: %.2f\n", \
				 num_matched, qdoc_len/k, (double)num_matched/(qdoc_len/k));
		 break;
	   
	    case RK:
		 /* for each chunk of qdoc (out of qdoc_len/k in total), 
		  * check if it appears in doc as a substring using 
		  * the rabin-karp substring matching algorithm */
		 num_matched = 0;
		 for (int i = 0; (i+k) <= qdoc_len; i += k) {
			 if (rabin_karp_match(qdoc+i, k, doc, doc_len)) {
				 num_matched++;
			 }
		 }
		 printf("%d chunks matched (out of %d), percentage: %.2f\n", \
				 num_matched, qdoc_len/k, (double)num_matched/(qdoc_len/k));
		 break;
	   
	    case RKBATCH:
		 /* match all qdoc_len/k chunks simultaneously (in batch) by using a bloom filter*/
		 num_matched = rabin_karp_batchmatch(((qdoc_len*10/k)>>3)<<3, k, \
				 qdoc, qdoc_len, doc, doc_len);
		 printf("%d chunks matched (out of %d), percentage: %.2f\n", \
				 num_matched, qdoc_len/k, (double)num_matched/(qdoc_len/k));
		 break;
	   
	    default :
		 fprintf(stderr,"Wrong algorithm type, choose from 0 1 2 3\n");
		 exit(1);
	}

	free(qdoc);
	free(doc);

	return 0;
}
