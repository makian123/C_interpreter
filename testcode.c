int func() {
	return 1;
}

int main(){
	int tmp = sizeof(int);
	int var = 200000000;

	while (var > 0) {
		var = var - 1;
	}

	return tmp + var;
}