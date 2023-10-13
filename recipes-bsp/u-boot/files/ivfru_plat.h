#ifndef __IVFRU_PLAT_H__
#define __IVFRU_PLAT_H__

int ivfru_plat_read_from_board(enum ivfru_board board, void *mem, int offset, int size);
int ivfru_plat_write_to_board(enum ivfru_board board, void *mem, int size);

int ivfru_plat_read_from_location(enum ivfru_board board, void *location, void *mem, int offset, int size);
int ivfru_plat_write_to_location(enum ivfru_board board, void *location, void *mem, int offset, int size);

int ivfru_plat_set_memory_size(int size);

#endif // __IVFRU_PLAT_H__
