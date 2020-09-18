int global = 2;

void other() {
	global = 3;
}

void entry() {
	other();
}
