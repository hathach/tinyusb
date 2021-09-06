DBG =
#DBG = -DCMAKE_BUILD_TYPE=Debug

all b/raw_dump.elf: b
	(cd b ;\
		rm -rf raw_dump.elf ;\
		cmake .. -DFAMILY=rp2040 $(DBG) ;\
		make ;\
		cd .. ;\
	)

clean:
	rm -rf b

b:
	mkdir b

dbg: b/raw_dump.elf
	./dbg-tusbh.sh
