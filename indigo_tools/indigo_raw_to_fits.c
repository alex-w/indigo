// Copyright (c) 2021 Rumen G. Bogdanovski
// All rights reserved.
//
// You can use this software under the terms of 'INDIGO Astronomy
// open-source license' (see LICENSE.md).
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHORS 'AS IS' AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
// GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// version history
// 2.0 by Rumen Bogdanovski <rumen@skyarchive.org>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <fcntl.h>
#include <errno.h>
#include <glob.h>
#include <sys/stat.h>
#include <limits.h>
#include <indigo/indigo_bus.h>
#include <indigo/indigo_raw_utils.h>

int save_file(char *file_name, char *data, int size) {
#if defined(INDIGO_WINDOWS)
	int handle = open(file_name, O_CREAT | O_WRONLY | O_BINARY, 0);
#else
	int handle = open(file_name, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
#endif
	if (handle < 0) {
		return -1;
	}
	int res = write(handle, data, size);
	if (res < size) {
		close(handle);
		return -1;
	}
	close(handle);
	return 0;
}

int open_file(const char *file_name, char **data, int *size) {
	char msg[PATH_MAX];
	int image_size = 0;
	if (file_name == "") {
		errno = ENOENT;
		return -1;
	}
	FILE *file;
	file = fopen(file_name, "rb");
	if (file) {
		fseek(file, 0, SEEK_END);
		image_size = (size_t)ftell(file);
		fseek(file, 0, SEEK_SET);
		if (*data == NULL) {
			*data = (char *)malloc(image_size + 1);
		} else {
			*data = (char *)realloc(*data, image_size + 1);
		}
		*size = fread(*data, image_size, 1, file);
		fclose(file);
		*size = image_size;
		return 0;
	}
	return -1;
}

static void print_help(const char *name) {
	printf("INDIGO RAW to FITS conversion tool v.%d.%d-%s built on %s %s.\n", (INDIGO_VERSION_CURRENT >> 8) & 0xFF, INDIGO_VERSION_CURRENT & 0xFF, INDIGO_BUILD, __DATE__, __TIME__);
	printf("usage: %s [options] file1.raw [file2.raw ...]\n", name);
	printf("output files will have '.fits' suffix.\n");
	printf("options:\n"
	       "       -h  | --help   : print this help\n"
	       "       -q  | --quiet  : print errors and statistics\n"
	);
}

int main(int argc, char *argv[]) {
	bool not_quiet = true;
	if (argc < 2) {
		print_help(argv[0]);
		return 0;
	}

	int arg_base = 1;
	for (int i = arg_base; i < argc; i++) {
		if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
			print_help(argv[0]);
			return 0;
		} else if (!strcmp(argv[i], "-q") || !strcmp(argv[i], "--quiet")) {
			not_quiet = false;
		} else if (argv[i] != "-") {
			break;
		}
		arg_base++;
	}

	int succeeded = 0;
	int failed = 0;
	for (int i = arg_base; i < argc; i++) {
		glob_t globlist;
		if (glob(argv[i], GLOB_PERIOD, NULL, &globlist) == GLOB_NOSPACE || glob(argv[i], GLOB_PERIOD, NULL, &globlist) == GLOB_NOMATCH) {
			fprintf(stderr,"Pattern '%s' did not match any files\n", argv[i]);
			continue;
		}
		if (glob(argv[i], GLOB_PERIOD, NULL, &globlist) == GLOB_ABORTED) {
			continue;
		}

		int k = 0;
		while (globlist.gl_pathv[k]) {
			char *in_data = NULL;
			char *out_data = NULL;
			int size = 0;
			char infile_name[PATH_MAX];

			strncpy(infile_name, globlist.gl_pathv[k], PATH_MAX);
			int res = open_file(globlist.gl_pathv[k], &in_data, &size);
			if (res != 0) {
				fprintf(stderr, "Can not open '%s' : %s\n", infile_name, strerror(errno));
				if (in_data) free(in_data);
				failed++;
				k++;
				continue;
			}
			res = indigo_raw_to_fists(in_data, &out_data, &size);
			if (res != INDIGO_OK) {
				fprintf(stderr, "Can not convert '%s' to FITS: %s\n", infile_name, strerror(errno));
				if (in_data) free(in_data);
				if (out_data) free(out_data);
				failed++;
				k++;
				continue;
			}

			char outfile_name[PATH_MAX];
			strncpy(outfile_name, infile_name, PATH_MAX);
			/* relace replace suffix with .fits */
			char *dot = strrchr(outfile_name, '.');
			if (dot) {
				strcpy(dot, ".fits");
			} else {
				snprintf(outfile_name, PATH_MAX, "%s.fits", infile_name);
			}

			res = save_file(outfile_name, out_data, size);
			if (res != 0) {
				fprintf(stderr, "Can not save '%s': %s\n", outfile_name, strerror(errno));
				failed++;
			} else {
				not_quiet && printf("Converted '%s' -> '%s'\n", infile_name, outfile_name);
				succeeded++;
			}
			if (in_data) free(in_data);
			if (out_data) free(out_data);
			k++;
		}
		globfree(&globlist);
	}
	fprintf(stderr, "%3d raw files sucessfully converted\n", succeeded);
	if (failed) {
		fprintf(stderr, "%3d files failed.\n", failed);
		return 1;
	}
	return 0;
}
