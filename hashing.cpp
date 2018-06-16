#include <iostream>
#include <cstring>
#include <cstdio>
#include <vector>
#include <bitset>
#include <cmath>

#define PAGE_SIZE 3
#define KEY_SIZE 8

using namespace std;

static int TABLE_DEPTH = 0;
static int HEAP_SIZE = 0;
static int REG_SIZE = 0;
static int PAGE_NUM = 0;

static bool DEBUG = false;

struct reg_t{
	int key;
	char name[20];
	int age;
};

struct page_t{
	int depth;
	int offset;
	int occupied;
};

struct node_t{
	int key;
	int page;
};

vector<node_t> vTable;
vector<page_t> vPages;
vector<int> vLeaves;

// >>>>>> CONSIDERAÇÕES <<<<<<

/*

	>> Estrutura do hash.bin:

		-> (int) tabledepth
		-> (int) pagenum
		-> (int[2^tabledepth]) vLeaves [[páginas para as quais cada folha aponta]]
		-> (page_t[pagenum]) pages
			-> cada page é composta de:
			-> (int) depth
			-> (int) offset
			-> (int) occupied

*/

// >>>>>> FUNCIONALIDADES <<<<<<

bool insertReg(reg_t r);
bool searchReg(int query, bool print);
bool removeReg(int query);
void printFile();
void average();

// >>>>>> FUNCOES INTERNAS <<<<<<

int h(int key) { return (int)(key % (long long)(pow(2, KEY_SIZE))); }
	
void clearAllData(){

	// limpar hash.bin

	FILE* hashFile;
	hashFile = fopen("hash.bin", "w+");

	int base_tabledepth = 1;
	int base_pagenum = 2;
		fwrite(&base_tabledepth, sizeof(int), 1, hashFile);
		fwrite(&base_pagenum, sizeof(int), 1, hashFile);

	// o heap do arquivo vazio tem 2 folhas, então 2 páginas em branco

	int leaf_pages[2]; leaf_pages[0] = 0; leaf_pages[1] = 1;
		fwrite(leaf_pages, sizeof(int), 2, hashFile);

	// criar duas páginas em branco

	for (int i=0; i < base_pagenum; i++){
		page_t blankPage;
		blankPage.depth = 1;
		blankPage.offset = i * REG_SIZE * PAGE_SIZE;
		blankPage.occupied = 0;
		fwrite(&(blankPage.depth), sizeof(int), 1, hashFile);
		fwrite(&(blankPage.offset), sizeof(int), 1, hashFile);
		fwrite(&(blankPage.occupied), sizeof(int), 1, hashFile);
	}

	fclose(hashFile);

	// limpar data.bin

	FILE* dataFile;
	dataFile = fopen("data.bin", "w+");

	// salvar base_pagenum páginas em branco

	reg_t blankReg;
	blankReg.key = 0;
	string str = ""; strcpy(blankReg.name, str.c_str());
	blankReg.age = 0;

	for (int i=0;i<base_pagenum;i++){
		for (int j=0;j<PAGE_SIZE;j++){
			fwrite(&(blankReg.key), sizeof(int), 1, dataFile);
			fwrite(blankReg.name, sizeof(char), 20, dataFile);
			fwrite(&(blankReg.age), sizeof(int), 1, dataFile);
		}
	}

	fclose(dataFile);

	return;

}

void readHash(){

	FILE* hashFile;
	hashFile = fopen("hash.bin", "r+");

		fread(&(TABLE_DEPTH), sizeof(int), 1, hashFile);
		fread(&(PAGE_NUM), sizeof(int), 1, hashFile);

		vLeaves.clear();
		int leaves = pow(2, TABLE_DEPTH);
		int leafpage;
		for (int i=0;i<leaves;i++){
			fread(&leafpage, sizeof(int), 1, hashFile);
			vLeaves.push_back(leafpage);
		}

		vPages.clear();
		page_t newPage;
		for (int i=0; i<PAGE_NUM; i++){
			fread(&(newPage.depth), sizeof(int), 1, hashFile);
			fread(&(newPage.offset), sizeof(int), 1, hashFile);
			fread(&(newPage.occupied), sizeof(int), 1, hashFile);
			vPages.push_back(newPage);
		}

	fclose(hashFile);

	return;
}

void initializeHeap(){

	// criar heap

	vTable.clear();

	node_t root;
	root.key = -1;
	root.page = -1;
	vTable.push_back(root);

	for(int i=1; i <= TABLE_DEPTH; i++){ // cria um nível do heap
		for (int j=0; j < pow(2,i); j++){ // cria os nós dentro do nível
			node_t node;
			node.key = 0;
			node.page = -1;
			vTable.push_back(node);
			j++;

			node.key = 1;
			node.page = -1;
			vTable.push_back(node);
		}
	}

	// atualizar apontadores de página das folhas

	int leftmost = pow(2,TABLE_DEPTH) - 1;
	int rightmost = pow(2,TABLE_DEPTH+1) - 2;
	for (int i=leftmost; i<=rightmost; i++){
		vTable[i].page = vLeaves[i-leftmost];
	}

	return;

}

void updateHashFile(){

	FILE* hashFile;
	hashFile = fopen("hash.bin", "w+");

		PAGE_NUM = vPages.size();

		fwrite(&TABLE_DEPTH, sizeof(int), 1, hashFile);
		fwrite(&PAGE_NUM, sizeof(int), 1, hashFile);

		int leaf;
		for (int i=0; i < pow(2,TABLE_DEPTH); i++){
			leaf = vLeaves[i];
			fwrite(&leaf, sizeof(int), 1, hashFile);
		}

		page_t page;
		for (int i=0; i<PAGE_NUM; i++){
			page = vPages[i];
			fwrite(&(page.depth), sizeof(int), 1, hashFile);
			fwrite(&(page.offset), sizeof(int), 1, hashFile);
			fwrite(&(page.occupied), sizeof(int), 1, hashFile);
		}

	fclose(hashFile);

	return;
}

int getNodeFromKey(int key){

	bitset<KEY_SIZE> bit(h(key));
	int pos = 0;

	for (int i=0;i<TABLE_DEPTH;i++){
		if (bit[KEY_SIZE-i-1] == 0) pos = (2*pos) + 1;
		else pos = (2*pos) + 2;
	}

	return pos;
}

void splitPage(int node){

	// criar uma row de nós na base e aumentar a profundidade da tabela

	int pageSplit = vTable[node].page;

	for (int i=0; i < pow(2,TABLE_DEPTH+1); i++){
		node_t newLeaf;
		newLeaf.key = 0;
		newLeaf.page = -1;
		vTable.push_back(newLeaf);
		i++;

		newLeaf.key = 1;
		vTable.push_back(newLeaf);
	}

	TABLE_DEPTH++;

	// atualizar os ponteiros para páginas
	// todas as folhas novas apontam para a mesma página do pai por enquanto

	int leftLeaf = pow(2,TABLE_DEPTH) - 1;
	int rightLeaf = pow(2,TABLE_DEPTH+1) - 2;
	for (int i=leftLeaf; i <= rightLeaf; i++){
		vTable[i].page = vTable[(int)((i-1)/2)].page;
	}

	vector<int> newLeaves((2*vLeaves.size()), -1);
	for (int i=0;i<vLeaves.size();i++){
		newLeaves[2*i] = vLeaves[i];
		newLeaves[2*i + 1] = vLeaves[i];
	}

	vLeaves.clear();
	for (int i=0; i < newLeaves.size(); i++){
		vLeaves.push_back(newLeaves[i]);
	}

	// resgatar a página que será dividida

	reg_t page[PAGE_SIZE];

	FILE* dataFile;
	dataFile = fopen("data.bin", "r+");

		int readOffset = vPages[pageSplit].offset;
		fseek(dataFile, readOffset, SEEK_SET);
		fread(page, REG_SIZE, PAGE_SIZE, dataFile);

	fclose(dataFile);

	// sobrescrever no arquivo com uma página em branco e atualizar occupied e depth

	dataFile = fopen("data.bin", "r+");

		fseek(dataFile, readOffset, SEEK_SET);

		reg_t blankReg;
		blankReg.key = 0;
		string str = ""; strcpy(blankReg.name, str.c_str());
		blankReg.age = 0;

		for(int i=0;i<PAGE_SIZE;i++){
			fwrite(&(blankReg.key), sizeof(int), 1, dataFile);
			fwrite(blankReg.name, sizeof(char), 20, dataFile);
			fwrite(&(blankReg.age), sizeof(int), 1, dataFile);
		}

		vPages[pageSplit].occupied = 0;
		vPages[pageSplit].depth++;

	fclose(dataFile);

	// ambos os filhos do nó a ser dividido já apontam para a mesma página do pai, que é a que acabamos de esvaziar.
	// o filho 0 continuará apontando para ela, enquanto o filho 1 receberá uma página nova.

	// criar página em branco no arquivo

	dataFile = fopen("data.bin", "a+");

		for(int i=0;i<PAGE_SIZE;i++){
			fwrite(&(blankReg.key), sizeof(int), 1, dataFile);
			fwrite(blankReg.name, sizeof(char), 20, dataFile);
			fwrite(&(blankReg.age), sizeof(int), 1, dataFile);
		}

	fclose(dataFile);

	// criar página em vPages e atualizar PAGE_NUM

	page_t newPage;
	newPage.depth = vPages[pageSplit].depth;
	newPage.offset = PAGE_NUM * REG_SIZE * PAGE_SIZE;
	newPage.occupied = 0;
	vPages.push_back(newPage);
	PAGE_NUM = vPages.size();

	// atualizar ponteiro de página do filho 1

	int nodeUpdate = 2*node + 2;
	vTable[nodeUpdate].page = PAGE_NUM - 1;
	vLeaves[nodeUpdate - leftLeaf] = PAGE_NUM - 1;

	// reinserir registros da página resgatada

	for (int i=0;i<PAGE_SIZE;i++){
		insertReg(page[i]);
	}

	return;
}

void mergePages(int queryNode, int queryPage){

	return;
}

// >>>>>> FUNÇÃO MAIN <<<<<<

int main(){

	// iniciar variáveis globais:
	REG_SIZE = sizeof(reg_t);

	//cout << "\n>> BEGINNING OF PROGRAM <<\n";

	clearAllData();

	readHash();
	initializeHeap();

	//cout << "\n>> BEGINNING OF QUERIES <<\n";

	char command;
	reg_t r;
	int query;

	while(cin.good()){
		cin >> command;
		if (command == 'i'){
			cin >> r.key;
			string str, trash;
			getline(cin, trash); getline(cin, str);
			strcpy(r.name, str.c_str());
			cin >> r.age;
			getline(cin, trash);

			bool success = insertReg(r);
			if (!success)
				cout << "chave ja existente: " << r.key << "\n";
		}
		if (command == 'c'){
			cin >> query;
			searchReg(query, true);
		}
		if (command == 'r'){
			cin >> query;

			bool success = removeReg(query);
			if (!success)
				cout << "chave nao encontrada: " << query << "\n";
		}
		if (command == 'p'){
			printFile();
		}
		if (command == 'm'){
			average();
		}
		if (command == 'e'){
			break;
		}
	}

	//cout << "\n>> END OF QUERIES <<\n";

	updateHashFile();

	//cout << "\n>> END OF PROGRAM <<\n";

	return 0;

}

bool insertReg(reg_t r){

	if (searchReg(r.key, false)){
		return false;
	}

	int queryNode = getNodeFromKey(r.key);
	int queryPage = vTable[queryNode].page;

	if (vPages[queryPage].occupied == PAGE_SIZE){
		splitPage(queryNode);
		queryNode = getNodeFromKey(r.key);
		queryPage = vTable[queryNode].page;
	}

	int writeOffset = vPages[queryPage].offset + (REG_SIZE * vPages[queryPage].occupied);

	FILE* dataFile;
	dataFile = fopen("data.bin", "r+");

		fseek(dataFile, writeOffset, SEEK_SET);
		fwrite(&(r.key), sizeof(int), 1, dataFile);
		fwrite(r.name, sizeof(char), sizeof(r.name), dataFile);
		fwrite(&(r.age), sizeof(int), 1, dataFile);
		vPages[queryPage].occupied++;

	fclose(dataFile);

	return true;
}

bool searchReg(int query, bool print){

	// calcular folha e página correspondentes à chave

	int queryNode = getNodeFromKey(query);
	int queryPage = vTable[queryNode].page;

	// buscar página na memória

	reg_t page[PAGE_SIZE];

	FILE* dataFile;
	dataFile = fopen("data.bin", "r+");

		fseek(dataFile, vPages[queryPage].offset, SEEK_SET);
		fread(page, REG_SIZE, PAGE_SIZE, dataFile);

	fclose(dataFile);

	// procurar query na página

	for (int i=0;i<PAGE_SIZE;i++){
		if (page[i].key == query) {
			if (print) {
				cout << "chave: " << query << "\n" << page[i].name << "\n" << page[i].age << "\n";
			}
			return true;
		}
	}

	if (print){
		cout << "chave nao encontrada: " << query << "\n";
	}
	return false;

}

void printFile(){

	cout << TABLE_DEPTH << "\n";

	string key;
	int leftLeaf = pow(2,TABLE_DEPTH) - 1;
	int rightLeaf = pow(2,TABLE_DEPTH+1) - 2;

	for (int i = leftLeaf; i <= rightLeaf; i++){
		key = "";
		int j = i;
		while (vTable[j].key != -1) {
			key = (char)(vTable[j].key + 48) + key;
			j = (j-1)/2;
		}
		cout << key << " ";
		if (vPages[vTable[i].page].occupied == 0) cout << "nulo";
		else cout << vTable[i].page;
		cout << "\n";

	}

	for (int i=0;i<PAGE_NUM;i++){
		cout << vPages[i].depth << "\n";

		reg_t page[PAGE_SIZE];

		FILE* dataFile;
		dataFile = fopen("data.bin", "r+");

			fseek(dataFile, vPages[i].offset, SEEK_SET);
			fread(page, REG_SIZE, PAGE_SIZE, dataFile);

		fclose(dataFile);

		int min = 0, mid = 0, max = 0;
		for (int i=1; i<3; i++){
			if (page[min].key == 0 && page[i].key > 0) min = i;
			else if (page[i].key < page[min].key && page[i].key > 0) min = i;
		}
		for (int i=1; i<3; i++){
			if (page[i].key > page[max].key || page[i].key == 0) max = i;
		}
		mid = 3 - (min + max);

		cout << page[min].key << " " << page[min].name << " " << page[min].age << "\n";
		cout << page[mid].key << " " << page[mid].name << " " << page[mid].age << "\n";
		cout << page[max].key << " " << page[max].name << " " << page[max].age << "\n";
	}

	return;
}

void average(){

	int sumAvg = 0;
	int count = 0;

	reg_t page[PAGE_SIZE];

	FILE* dataFile;
	dataFile = fopen("data.bin", "r+");

		for (int i=0;i<vPages.size();i++){

			fseek(dataFile, vPages[i].offset, SEEK_SET);			
			fread(page, REG_SIZE, PAGE_SIZE, dataFile);

			for (int i=0;i<PAGE_SIZE;i++){
				if (page[0].key != 0) {
					count++;
					sumAvg += i + 1;
				}
			}
		}

	fclose(dataFile);

	double avg; 
	if (count == 0) avg = 0;
	else avg = sumAvg/count;

	cout << avg << "\n";

	return;
}

bool removeReg(int query){

	if (!searchReg(query, false)){
		return false;
	}

	int queryNode = getNodeFromKey(query);
	int queryPage = vTable[queryNode].page;

	// buscar página na memória

	reg_t page[PAGE_SIZE];

	FILE* dataFile;
	dataFile = fopen("data.bin", "r+");

		fseek(dataFile, vPages[queryPage].offset, SEEK_SET);
		fread(page, REG_SIZE, PAGE_SIZE, dataFile);

	// encontrar posição do registro, puxar os registros seguintes uma posição acima e salvar um registro vazio
	int position;
	for (int i=0;i<PAGE_SIZE;i++){
		if (page[i].key == query) {
			position = i;
		}
	}

	for (int i=position;i<PAGE_SIZE-1;i++){
		page[i] = page[i+1];
	}

	reg_t blankReg;
	blankReg.key = 0;
	string str = ""; strcpy(blankReg.name, str.c_str());
	blankReg.age = 0;

	page[PAGE_SIZE-1] = blankReg;

	// sobrescrever página no arquivo

		fseek(dataFile, vPages[queryPage].offset, SEEK_SET);

		for (int i=0;i<PAGE_NUM;i++){
			fwrite(&(page[i].key), sizeof(int), 1, dataFile);
			fwrite(page[i].name, sizeof(char), 20, dataFile);
			fwrite(&(page[i].age), sizeof(int), 1, dataFile);
		}

	fclose(dataFile);

	// diminuir ocupação

	vPages[queryPage].occupied--;

	mergePages(queryNode, queryPage);

	return true;

}