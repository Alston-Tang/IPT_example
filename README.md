# IPT_example

build and run:
```
make
sudo ./pt
```
---------------------------------

change aux buffer size in the code:
```
mmap_page->aux_size = [SIZE];
```
[SIZE] has to be 2^n where n is postive integer

---------------------------------
you may need to change attr type on your platform
```
attr.type = 8;
```
make sure `attr.type=$(/sys/devices/intel_pt/type);

