#ifndef __SIZES_H__
#define __SIZES_H__

#ifdef __cplusplus
extern "C" {
#endif

#define SZ_16				(0x00000010)
#define SZ_256				(0x00000100)
#define SZ_512				(0x00000200)

#define SZ_1K				(0x00000400)
#define SZ_4K				(0x00001000)
#define SZ_8K				(0x00002000)
#define SZ_16K				(0x00004000)
#define SZ_32K				(0x00008000)
#define SZ_64K				(0x00010000)
#define SZ_128K				(0x00020000)
#define SZ_256K				(0x00040000)
#define SZ_512K				(0x00080000)

#define SZ_1M				(0x00100000)
#define SZ_2M				(0x00200000)
#define SZ_4M				(0x00400000)
#define SZ_8M				(0x00800000)
#define SZ_16M				(0x01000000)
#define SZ_32M				(0x02000000)
#define SZ_64M				(0x04000000)
#define SZ_128M				(0x08000000)
#define SZ_256M				(0x10000000)
#define SZ_512M				(0x20000000)

#define SZ_1G				(0x40000000)
#define SZ_2G				(0x80000000)

#define ARRAY_SIZE(array)	( sizeof(array) / sizeof((array)[0]) )

#ifdef __cplusplus
}
#endif

#endif /* __SIZES_H__ */
