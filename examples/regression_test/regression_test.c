/*
*	NVFUSE (NVMe based File System in Userspace)
*	Copyright (C) 2016 Yongseok Oh <yongseok.oh@sk.com>
*	First Writing: 30/10/2016
*
* This program is free software; you can redistribute it and/or modify it
* under the terms and conditions of the GNU General Public License,
* version 2, as published by the Free Software Foundation.
*
* This program is distributed in the hope it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
* more details.
*/

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>

#include "nvfuse_core.h"
#include "nvfuse_api.h"
#include "nvfuse_io_manager.h"
#include "nvfuse_malloc.h"
#include "nvfuse_gettimeofday.h"
#include "nvfuse_aio.h"

#define DEINIT_IOM	1
#define UMOUNT		1

#define NUM_ELEMENTS(x) (sizeof(x)/sizeof(x[0]))

#define MB (1024*1024)
#define GB (1024*1024*1024)
#define TB ((s64)1024*1024*1024*1024)

#define RT_TEST_TYPE MILL_TEST

#define MAX_TEST    1
/* quick test */
#define QUICK_TEST  2
/* 1 million create/delete test */
#define MILL_TEST   3

static s32 last_percent;
static s32 test_type = QUICK_TEST;

void rt_progress_reset()
{
    last_percent = 0;
}

void rt_progress_report(s32 curr, s32 max)
{
    int curr_percent;

    curr_percent = (curr + 1) * 100 / max;

    if	(curr_percent != last_percent)
    {
		last_percent = curr_percent;
		printf(".");
		if (curr_percent % 10 == 0)
		    printf("%d%%\n", curr_percent);
		fflush(stdout);
    }
}

char *rt_decode_test_type(s32 type)
{
	switch (type)
	{
		case MAX_TEST:
			return "MAX_TEST";
		case QUICK_TEST:
			return "QUICK_TEST";
		case MILL_TEST:
			return "MILL_TEST";
	}

	return NULL;
}

int rt_create_files(struct nvfuse_handle *nvh, u32 arg)
{
	struct timeval tv;
	struct statvfs stat;
	s8 buf[FNAME_SIZE];
	s32 max_inodes;
	s32 i;
	s32 fd;
	s32 res;

	if (nvfuse_statvfs(nvh, NULL, &stat) < 0)
	{
		printf(" statfs error \n");
		return -1;
	}

	switch (test_type)
	{
		case MAX_TEST:
			max_inodes = stat.f_ffree; /* # of free inodes */
			break;
		case QUICK_TEST:
			max_inodes = 100;
			break;
		case MILL_TEST:
			/* # of free inodes */
			max_inodes = stat.f_ffree < 1000000 ? stat.f_ffree: 1000000;			
			break;
		default:
			printf(" Invalid test type = %d\n", test_type);
			return -1;
	}
	
	/* reset progress percent */
	rt_progress_reset();
	gettimeofday(&tv, NULL);

	/* create null files */
	printf(" Start: creating null files (0x%x).\n", max_inodes);
	for (i = 0; i < max_inodes; i++)
	{
		sprintf(buf, "file%d\n", i);

		fd = nvfuse_openfile_path(nvh, buf, O_RDWR | O_CREAT, 0);
		if (fd == -1) {
			printf(" Error: open() \n");
			return -1;
		}
		nvfuse_closefile(nvh, fd);
		/* update progress percent */
		rt_progress_report(i, max_inodes);
	}
	printf(" Finish: creating null files (0x%x) %.3f OPS.\n", max_inodes, max_inodes / time_since_now(&tv));

	/* reset progress percent */
	rt_progress_reset();
	gettimeofday(&tv, NULL);

	/* lookup null files */
	printf(" Start: looking up null files (0x%x).\n", max_inodes);
	for (i = 0; i < max_inodes; i++)
	{
		struct stat st_buf;
		int res;

		sprintf(buf, "file%d\n", i);

		res = nvfuse_getattr(nvh, buf, &st_buf);
		if (res) 
		{
			printf(" No such file %s\n", buf);
			return -1;
		}
		/* update progress percent */
		rt_progress_report(i, max_inodes);
	}
	printf(" Finish: looking up null files (0x%x) %.3f OPS.\n", max_inodes, max_inodes / time_since_now(&tv));

	/* reset progress percent */
	rt_progress_reset();
	gettimeofday(&tv, NULL);

	/* delete null files */
	printf(" Start: deleting null files (0x%x).\n", max_inodes);
	for (i = 0; i < max_inodes; i++)
	{
		sprintf(buf, "file%d\n", i);

		res = nvfuse_rmfile_path(nvh, buf);
		if (res)
		{
			printf(" rmfile = %s error \n", buf);
			return -1;
		}
		/* update progress percent */
		rt_progress_report(i, max_inodes);
	}
	printf(" Finish: deleting null files (0x%x) %.3f OPS.\n", max_inodes, max_inodes / time_since_now(&tv));

	return 0;
}

int rt_create_dirs(struct nvfuse_handle *nvh, u32 arg)
{
	struct timeval tv;
	struct statvfs stat;
	s8 buf[FNAME_SIZE];
	s32 max_inodes;
	s32 i;
	s32 res;

	if (nvfuse_statvfs(nvh, NULL, &stat) < 0)
	{
		printf(" statfs error \n");
		return -1;
	}

	switch (test_type)
	{
		case MAX_TEST:
			max_inodes = stat.f_ffree; /* # of free inodes */
			break;
		case QUICK_TEST:
			max_inodes = 100;
			break;
		case MILL_TEST:
			/* # of free inodes */
			max_inodes = stat.f_ffree < 1000000 ? stat.f_ffree: 1000000;			
			break;
		default:
			printf(" Invalid test type = %d\n", test_type);
			return -1;			
	}

	/* reset progress percent */
	rt_progress_reset();
	gettimeofday(&tv, NULL);

	/* create null directories */
	printf(" Start: creating null directories (0x%x).\n", max_inodes);
	for (i = 0; i < max_inodes; i++)
	{
		sprintf(buf, "dir%d\n", i);
		res = nvfuse_mkdir_path(nvh, buf, 0644);
		if (res < 0)
		{
			printf(" Error: create dir = %s \n", buf);
			return res;
		}
		/* update progress percent */
		rt_progress_report(i, max_inodes);

	}
	printf(" Finish: creating null directories (0x%x) %.3f OPS.\n", max_inodes, max_inodes / time_since_now(&tv));

	/* reset progress percent */
	rt_progress_reset();
	gettimeofday(&tv, NULL);
	/* lookup null directories */
	printf(" Start: looking up null directories (0x%x).\n", max_inodes);
	for (i = 0; i < max_inodes; i++)
	{
		struct stat st_buf;
		int res;

		sprintf(buf, "dir%d\n", i);

		res = nvfuse_getattr(nvh, buf, &st_buf);
		if (res) 
		{
			printf(" No such directory %s\n", buf);
			return -1;
		}
		/* update progress percent */
		rt_progress_report(i, max_inodes);
	}
	printf(" Finish: looking up null directories (0x%x) %.3f OPS.\n", max_inodes, max_inodes / time_since_now(&tv));

	/* reset progress percent */
	rt_progress_reset();
	gettimeofday(&tv, NULL);
	/* delete null directories */
	printf(" Start: deleting null directories (0x%x).\n", max_inodes);
	for (i = 0; i < max_inodes; i++)
	{
		sprintf(buf, "dir%d\n", i);

		res = nvfuse_rmdir_path(nvh, buf);
		if (res)
		{
			printf(" rmfile = %s error \n", buf);
			return -1;
		}
		/* update progress percent */
		rt_progress_report(i, max_inodes);
	}
	printf(" Finish: deleting null files (0x%x) %.3f OPS.\n", max_inodes, max_inodes / time_since_now(&tv));

	return 0;
}

int rt_create_max_sized_file(struct nvfuse_handle *nvh, u32 arg)
{
	struct statvfs statvfs_buf;
	struct stat stat_buf;
	char str[128];
	struct timeval tv;
	s64 file_size;
	s64 file_allocated_size;
	s32 res;
	s32 fid;

	if (nvfuse_statvfs(nvh, NULL, &statvfs_buf) < 0)
	{
		printf(" statfs error \n");
		return -1;
	}

	sprintf(str, "file_allocate_test");

	switch (test_type)
	{
		case MAX_TEST:
			file_size = (s64) statvfs_buf.f_bfree * CLUSTER_SIZE;
			break;
		case QUICK_TEST:
			file_size = 100 * MB;
			break;
		case MILL_TEST:
			file_size = (s64)1 * TB;
			file_size = (file_size > (s64)statvfs_buf.f_bfree * CLUSTER_SIZE) ?	
				(s64)(statvfs_buf.f_bfree / 2) * CLUSTER_SIZE : 
				(s64)file_size;
			break;
		default:
			printf(" Invalid test type = %d\n", test_type);
			return -1;
	}

	fid = nvfuse_openfile_path(nvh, str, O_RDWR | O_CREAT, 0);
	if (fid < 0)
	{
		printf(" Error: file open or create \n");
		return -1;
	}
	nvfuse_closefile(nvh, fid);

	gettimeofday(&tv, NULL);
	printf("\n Start: Fallocate and Deallocate (file %s size %luMB). \n", str, (long)file_size/MB);
	/* pre-allocation of data blocks*/
	nvfuse_fallocate(nvh, str, 0, file_size);

	res = nvfuse_getattr(nvh, str, &stat_buf);
	if (res) 
	{
		printf(" No such file %s\n", str);
		return -1;
	}

	/* NOTE: Allocated size may differ from requested size. */
	file_allocated_size = stat_buf.st_size;

	printf(" requested size %dMB.\n", (long)file_size/MB);
	printf(" allocated size %dMB.\n", (long)file_allocated_size/MB); 

	printf(" nvfuse fallocate throughput %.3fMB/s (%0.3fs).\n", (double)file_allocated_size/MB/time_since_now(&tv), time_since_now(&tv));

	gettimeofday(&tv, NULL);
	printf(" Start: rmfile %s size %luMB \n", str, (long)file_allocated_size/MB);
	res = nvfuse_rmfile_path(nvh, str);
	if (res < 0)
	{
		printf(" Error: rmfile = %s\n", str);
		return -1;
	}
	printf(" nvfuse rmfile throughput %.3fMB/s\n", (double)file_allocated_size/MB/time_since_now(&tv));

	printf("\n Finish: Fallocate and Deallocate.\n");

	return NVFUSE_SUCCESS;
}

int rt_gen_aio_rw(struct nvfuse_handle *nvh, s64 file_size, s32 block_size, s32 is_rand, s32 direct, s32 qdepth)
{	
	struct timeval tv;
	char str[FNAME_SIZE];
	s32 res;

	sprintf(str, "file_allocate_test");

	gettimeofday(&tv, NULL);

	/* write phase */
	{
		res = nvfuse_aio_test_rw(nvh, str, file_size, block_size, qdepth, WRITE, direct, is_rand);
		if (res < 0)
		{
			printf(" Error: aio write test \n");
			goto AIO_ERROR;
		}
		printf(" nvfuse aio write through %.3f MB/s\n", (double)file_size / MB / time_since_now(&tv));

		res = nvfuse_rmfile_path(nvh, str);
		if (res < 0)
		{
			printf(" Error: rmfile = %s\n", str);
			return -1;
		}
	}

	gettimeofday(&tv, NULL);
	/* read phase */
	{
		res = nvfuse_aio_test_rw(nvh, str, file_size, block_size, qdepth, READ, direct, is_rand);
		if (res < 0)
		{
			printf(" Error: aio read test \n");
			goto AIO_ERROR;
		}
		printf(" nvfuse aio read through %.3f MB/s\n", (double)file_size / MB / time_since_now(&tv));

		res = nvfuse_rmfile_path(nvh, str);
		if (res < 0)
		{
			printf(" Error: rmfile = %s\n", str);
			return -1;
		}
	}

	return 0;

AIO_ERROR:	
	res = nvfuse_rmfile_path(nvh, str);
	if (res < 0)
	{
	    printf(" Error: rmfile = %s\n", str);
	    return -1;
	}
	return -1;
}

int rt_create_max_sized_file_aio_4KB(struct nvfuse_handle *nvh, u32 is_rand)
{
	struct statvfs statvfs_buf;
	s32 direct;
	s32 qdepth;
	s32 res;	
	s64 file_size;
	s32 block_size;
	
	if (nvfuse_statvfs(nvh, NULL, &statvfs_buf) < 0)
	{
		printf(" statfs error \n");
		return -1;
	}

	switch (test_type)
	{
		case MAX_TEST:
			file_size = (s64) statvfs_buf.f_bfree * CLUSTER_SIZE;
			break;
		case QUICK_TEST:
			file_size = 100 * MB;
			break;
		case MILL_TEST:
			file_size = (s64)128 * GB;
			file_size = (file_size > (s64)statvfs_buf.f_bfree * CLUSTER_SIZE) ?	
				(s64)(statvfs_buf.f_bfree / 2) * CLUSTER_SIZE : 
				(s64)file_size;
			break;
		default:
			printf(" Invalid test type = %d\n", test_type);
			return -1;
	}

	direct = 1;
	qdepth = 128;
	block_size = 4096;
	res = rt_gen_aio_rw(nvh, file_size, block_size, is_rand, direct, qdepth);

	return NVFUSE_SUCCESS;
}

int rt_create_max_sized_file_aio_128KB(struct nvfuse_handle *nvh, u32 is_rand)
{
	struct statvfs statvfs_buf;
	s32 direct;
	s32 qdepth;
	s32 res;
	s64 file_size;
	s32 block_size;

	if (nvfuse_statvfs(nvh, NULL, &statvfs_buf) < 0)
	{
		printf(" statfs error \n");
		return -1;
	}

	switch (test_type)
	{
		case MAX_TEST:
			file_size = (s64) statvfs_buf.f_bfree * CLUSTER_SIZE;
			break;
		case QUICK_TEST:
			file_size = 100 * MB;
			break;
		case MILL_TEST:
			file_size = (s64)128 * GB;
			file_size = (file_size > (s64)statvfs_buf.f_bfree * CLUSTER_SIZE) ?	
				(s64)(statvfs_buf.f_bfree / 2) * CLUSTER_SIZE : 
				(s64)file_size;
			break;
		default:
			printf(" Invalid test type = %d\n", test_type);
			return -1;
	}

	direct = 1;
	qdepth = 128;
	block_size = 128 * 1024;
	res = rt_gen_aio_rw(nvh, file_size, block_size, is_rand, direct, qdepth);

	return NVFUSE_SUCCESS;
}

int rt_create_4KB_files(struct nvfuse_handle *nvh, u32 arg)
{
	struct timeval tv;
	struct statvfs statvfs_buf;
	char str[FNAME_SIZE];
	s32 res;
	s32 nr;
	int i;

	if (nvfuse_statvfs(nvh, NULL, &statvfs_buf) < 0)
	{
		printf(" statfs error \n");
		return -1;
	}

#if (RT_TEST_TYPE == MAX_TEST)
	
#elif (RT_TEST_TYPE == QUICK_TEST)
	
#elif (RT_TEST_TYPE == MILL_TEST)
	
#endif
	switch (test_type)
	{
		case MAX_TEST:
			nr = statvfs_buf.f_bfree / 2;
			break;
		case QUICK_TEST:
			nr = 100;
			break;
		case MILL_TEST:
			nr = statvfs_buf.f_bfree / 2;
			if (nr > 1000000)
			{
				nr = 1000000;
			}
			break;
		default:
			printf(" Invalid test type = %d\n", test_type);
			return -1;			
	}

	printf(" # of files = %d \n", nr);

	/* reset progress percent */
	rt_progress_reset();
	gettimeofday(&tv, NULL);

	printf(" Start: creating 4KB files (0x%x).\n", nr);
	/* create files*/
	for (i = 0; i < nr; i++)
	{
		sprintf(str, "file%d", i);
		res = nvfuse_mkfile(nvh, str, "4096");			
		if (res < 0)
		{
			printf(" mkfile error = %s\n", str);
			return -1;
		}

		/* update progress percent */
		rt_progress_report(i, nr);
	}		
	printf(" Finish: creating 4KB files (0x%x) %.3f OPS.\n", nr, nr / time_since_now(&tv));

	/* reset progress percent */
	rt_progress_reset();
	gettimeofday(&tv, NULL);

	printf(" Start: looking up 4KB files (0x%x).\n", nr);
	/* lookup files */
	for (i = 0; i < nr; i++)
	{
		struct stat st_buf;
		int res;

		sprintf(str, "file%d", i);
		res = nvfuse_getattr(nvh, str, &st_buf);
		if (res)
		{
			printf(" No such file %s\n", str);
			return -1;
		}
		/* update progress percent */
		rt_progress_report(i, nr);
	}
	printf(" Finish: looking up 4KB files (0x%x) %.3f OPS.\n", nr, nr / time_since_now(&tv));

	/* reset progress percent */
	rt_progress_reset();
	gettimeofday(&tv, NULL);

	/* delete files */
	printf(" Start: deleting 4KB files (0x%x).\n", nr);
	for (i = 0; i < nr; i++)
	{
		sprintf(str, "file%d", i);
		res = nvfuse_rmfile_path(nvh, str);
		if (res < 0)
		{
			printf(" rmfile error = %s \n", str);
			return -1;
		}
		/* update progress percent */
		rt_progress_report(i, nr);
	}
	printf(" Finish: deleting 4KB files (0x%x) %.3f OPS.\n", nr, nr / time_since_now(&tv));

	return NVFUSE_SUCCESS;

}

#define RANDOM		1
#define SEQUENTIAL	0

struct regression_test_ctx
{
	s32 (*function)(struct nvfuse_handle *nvh, u32 arg);
	s8 test_name[128];
	s32 arg;
	s32 pass_criteria; /* compare return code */
	s32 pass_criteria_ignore; /* no compare */
} 
rt_ctx[] = 
{
	{ rt_create_files, "Creating Max Number of Files.", 0, 0, 0},
	{ rt_create_dirs, "Creating Max Number of Directories.", 0, 0, 0},
	{ rt_create_max_sized_file, "Creating Maximum Sized Single File.", 0, 0, 0},
	{ rt_create_max_sized_file_aio_4KB, "Creating Maximum Sized Single File with 4KB Sequential AIO Read and Write.", SEQUENTIAL, 0, 0},
	{ rt_create_max_sized_file_aio_4KB, "Creating Maximum Sized Single File with 4KB Random AIO Read and Write.", RANDOM, 0, 0},
	{ rt_create_max_sized_file_aio_128KB, "Creating Maximum Sized Single File with 128KB Sequential AIO Read and Write.", SEQUENTIAL, 0, 0 },
	{ rt_create_max_sized_file_aio_128KB, "Creating Maximum Sized Single File with 128KB Random AIO Read and Write.", RANDOM, 0, 0 },
	{ rt_create_4KB_files, "Creating 4KB files with fsync.", 0, 0, 0}
};

void rt_usage(char *cmd)
{
	printf("\nOptions for NVFUSE application: \n");
	printf("\t-T: test type (e.g., 1: max_test, 2: quick_test, 3: million test \n");	
}

int main(int argc, char *argv[])
{
	struct nvfuse_io_manager io_manager;
	struct nvfuse_ipc_context ipc_ctx;
	struct nvfuse_params params;
	struct nvfuse_handle *nvh;
	struct regression_test_ctx *cur_rt_ctx;	
	int ret = 0;

	int core_argc = 0;
	char *core_argv[128];		
	int app_argc = 0;
	char *app_argv[128];
	char op;

	/* distinguish cmd line into core args and app args */
	nvfuse_distinguish_core_and_app_options(argc, argv, 
											&core_argc, core_argv, 
											&app_argc, app_argv);
	
	ret = nvfuse_parse_args(core_argc, core_argv, &params);
	if (ret < 0)
		return -1;
	
	if (__builtin_popcount((u32)params.cpu_core_mask) > 1)
	{
		printf(" This example is only executed on single core.\n");
		printf(" Given cpu core mask = %x \n", params.cpu_core_mask);
		return -1;
	}

	ret = nvfuse_configure_spdk(&io_manager, &ipc_ctx, params.cpu_core_mask, NVFUSE_MAX_AIO_DEPTH);
	if (ret < 0)
		return -1;

	/* optind must be reset before using getopt() */
	optind = 0;
	while ((op = getopt(app_argc, app_argv, "T:")) != -1) {
		switch (op) {	
		case 'T':
			test_type = atoi(optarg);
			if (test_type < MAX_TEST || test_type > MILL_TEST)
			{
				fprintf(stderr, " Invalid test type = %d", test_type);
				goto INVALID_ARGS;
			}
			break;
		default:
			goto INVALID_ARGS;
		}
	}
	
	printf(" Perform test %s ... \n", rt_decode_test_type(test_type));

	/* create nvfuse_handle with user spcified parameters */
	nvh = nvfuse_create_handle(&io_manager, &ipc_ctx, &params);
	if (nvh == NULL)
	{
		fprintf(stderr, "Error: nvfuse_create_handle()\n");
		return -1;
	}

	printf("\n");

	cur_rt_ctx = rt_ctx;

	/* Test Case Handler with Regression Test Context Array */
	while (cur_rt_ctx < rt_ctx + NUM_ELEMENTS(rt_ctx))
	{
		s32 index = cur_rt_ctx - rt_ctx + 1;
		printf(" Regression Test %d: %s\n", index, cur_rt_ctx->test_name);
		ret = cur_rt_ctx->function(nvh, cur_rt_ctx->arg);
		if (!cur_rt_ctx->pass_criteria && 
			ret != cur_rt_ctx->pass_criteria)
		{
			printf(" Failed Regression Test %d.\n", index);
			goto RET;
		}

		printf(" Regression Test %d: passed successfully.\n\n", index);
		cur_rt_ctx++;
	}

RET:;
	nvfuse_destroy_handle(nvh, DEINIT_IOM, UMOUNT);
	nvfuse_deinit_spdk(&io_manager, &ipc_ctx);

	return ret;

INVALID_ARGS:;
	nvfuse_core_usage(argv[0]);
	rt_usage(argv[0]);
	nvfuse_core_usage_example(argv[0]);
	return -1;
}
