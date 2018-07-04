#include <iostream>
#include <cstring>
#include <cstdio>
#include <vector>
#include <bitset>
#include <cmath>

using namespace std;

int main(){

	int n = 0;

	FILE* hashFile;
	hashFile = fopen("hash.bin", "r+");

	while (fgetc(hashFile) != EOF){
		++n;
	}

	if(feof(hashFile)){
		cout << "End of file reached.\n Total number of bytes read: " << n << "\n";
	}

	fclose(hashFile);

	return 0;
}