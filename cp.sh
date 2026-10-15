if [ $# -eq 0 ]; then
	echo "usage cp.sh [last no of ip]"
	exit 1
fi

pscp -pw gd@2023 ub.sh geduer@192.168.1.$1:/home/geduer
pscp -pw gd@2023 uboot.img geduer@192.168.1.$1:/home/geduer

