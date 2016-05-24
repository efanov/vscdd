main(){
	int fd = open("./zzz0", 0);
	lseek(fd, 3, 0);
	lseek(fd, 3, 1);
	lseek(fd, 3, 2);
        close();
}
