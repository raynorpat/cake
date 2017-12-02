@echo off

extractfuncts_x86 *.c -o tables\g_func_list.h tables\g_func_decs.h
extractfuncts_x86 *.c -t mmove_t -o tables\g_mmove_list.h tables\g_mmove_decs.h
