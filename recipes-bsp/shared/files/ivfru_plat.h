#ifndef __IVFRU_PLAT_H__
#define __IVFRU_PLAT_H__

int ivfru_plat_read_from_board(enum ivfru_board board, void *mem, int offset, int size, int quiet);
int ivfru_plat_write_to_board(enum ivfru_board board, void *mem, int size);

int ivfru_plat_read_from_location(void *location);
int ivfru_plat_write_to_location(void *location);

int ivfru_plat_set_buffer(void *buffer);
void *ivfru_plat_get_buffer();
int ivfru_plat_reserve_memory(int size);
int ivfru_plat_set_image_size(int size);
int ivfru_plat_get_image_size();

#endif // __IVFRU_PLAT_H__
