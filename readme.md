## iMentor

노트북에서 연결하기

```sh
# putty
ifconfig eth0 192.168.0.10
./nfsmount
mount -t nfs -o nolock,tcp,sw,bg 192.168.0.2:/sdk/nfs /nfs/

#wsl
sudo ifconfig eth0 192.168.0.2
ping 192.168.0.10
sudo service rpcbind start
sudo /etc/init.d/nfs-kernel-server restart

#이더넷 속성
192.168.0.20
255.255.255.0
192.168.0.1
168.126.63.1
168.126.63.2

#wsl 속성
192.168.0.2
255.255.255.0

# 이더넷, wsl 네트워크 연결 브리지
```



비트맵 파일 출력하기
