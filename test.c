int main(){
	int fd = open("./zzz1", 0);
	lseek(fd, 5, 0);
	lseek(fd, 5, 1);
	lseek(fd, 5, 2);
	return 0;
}
