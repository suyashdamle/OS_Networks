myfs_test_case1.out: myfs.h.gch myfs_test_case2.out myfs_test_case3.out myfs_test_case4.out 
	g++ myfs_test_case1.cpp  -o myfs_test_case1.out -std=gnu++11

myfs_test_case2.out: 
	g++ myfs_test_case2.cpp  -o myfs_test_case2.out -std=gnu++11

myfs_test_case3.out: 
	g++ myfs_test_case3.cpp  -o myfs_test_case3.out -std=gnu++11

myfs_test_case4.out: 
	g++ myfs_test_case4.cpp  -o myfs_test_case4.out -std=gnu++11

myfs.h.gch:
	g++ myfs.h  -std=gnu++11	

clean: 
	rm myfs.h.gch myfs_test_case1.out myfs_test_case2.out myfs_test_case3.out myfs_test_case4.out