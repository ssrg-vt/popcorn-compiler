#pragma once

int dsm_copy_stack(void* addr);
int send_page(char* arg, int size, void* data);
int send_pmap(char* arg, int size, void* data);
int dsm_control_access(int update, int first, int local);
