#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/bn.h>
 
void main(){
	BIGNUM *rnd;
	rnd = BN_new();
	
	int length;
	char * show;
	int bits = 128;
	int top =0;
	int bottom = 0;
	
	top = 0;
	bottom = 0;
	BN_rand(rnd,bits,top,bottom);	
	length = BN_num_bits(rnd);
	show = BN_bn2hex(rnd);
	printf("length:%d,rnd:%s(%d)\n",length,show, BN_num_bytes(rnd));
	BN_free(rnd);
}
