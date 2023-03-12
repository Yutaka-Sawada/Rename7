// Copyright : 2023-03-12 Yutaka Sawada
// License : The MIT license

// Visual Studio 2022 or MinGW is required to compile this.

// disable warning message of compiler
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <wchar.h>

#include <Windows.h>

// global variables
unsigned int cp_original;	// original Console Code Page
unsigned int cp_output = 0;	// current Console Output Code Page
int flag_yes = 0;
int flag_space = 0;
int flag_force = 0;
int flag_recurse = 0;
int flag_locale = 0;
int replace_count = 0;

// print UTF-16 text with the current Console Output Code Page
void printf_cp(unsigned char *format, wchar_t *unicode_text)
{
	unsigned int dwFlags, len, rv;
	char *new_text;

	len = wcslen(unicode_text) * 4 + 1;
	new_text = malloc(len);
	if (new_text == NULL){
		printf("Error: failed to convert Code Page.\n");
		return;
	}

	if (cp_output == CP_UTF8){
		dwFlags = 0;
	} else {
		dwFlags = WC_NO_BEST_FIT_CHARS;
	}

	rv = WideCharToMultiByte(cp_output, dwFlags, unicode_text, -1, new_text, len, NULL, NULL);
	if (rv == 0){
		if ((dwFlags != 0) && (GetLastError() == ERROR_INVALID_FLAGS))
			rv = WideCharToMultiByte(cp_output, 0, unicode_text, -1, new_text, len, NULL, NULL);
	}
	if (rv == 0){
		free(new_text);
		printf("Error: failed to convert Code Page.\n");
		return;
	}

	printf(format, new_text);
	free(new_text);
}

int value_to_char(int v)
{
	if (v >= 10)
		return 'A' + v - 10;
	return '0' + v;
}

int char_to_value(int v)
{
	if (v >= '0'){
		if (v <= '9')
			return v - '0';
		if (v >= 'A'){
			if (v <= 'F')
				return v - 'A' + 10;
		}
	}

	return -1;
}

int check_char(wchar_t *in)
{
	int ret, len;
	char out[8];
	wchar_t out2[8];

	// first check of conversion
	ret = WideCharToMultiByte(cp_original, WC_NO_BEST_FIT_CHARS, in, 1, out, 8, NULL, NULL);
	if (ret == 0){
		if (GetLastError() == ERROR_INVALID_FLAGS)
			ret = WideCharToMultiByte(cp_original, 0, in, 1, out, 8, NULL, NULL);
	}
	if (ret == 0)
		return -1;

	// second check of reverse
	len = ret;
	ret = MultiByteToWideChar(cp_original, MB_ERR_INVALID_CHARS, out, len, out2, 8);
	if (ret == 0){
		if (GetLastError() == ERROR_INVALID_FLAGS)
			ret = MultiByteToWideChar(cp_original, 0, out, len, out2, 8);
	}
	if (ret != 1)
		return -1;

	// final check of validation
	if (in[0] != out2[0])
		return -1;

	return 1;
}

int search_dir_check(wchar_t *parent_dir)
{
	int i, j, v;
	wchar_t new_name[MAX_PATH * 5], *old_name, *child_dir;
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind;

	// Find first file in current directory
	i = wcslen(parent_dir);
	wcscpy(parent_dir + i, L"*");
	hFind = FindFirstFile(parent_dir, &FindFileData);
	parent_dir[i] = 0;
	if (hFind == INVALID_HANDLE_VALUE)
		return 0;
	while (hFind != INVALID_HANDLE_VALUE){
		old_name = FindFileData.cFileName;

		// Exclude this and paretnt directories
		// Exclude hidden, read-only, and system files
		if ((wcscmp(old_name, L".") == 0) || (wcscmp(old_name, L"..") == 0)
				|| ((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0)
				|| ((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0)
				|| ((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) != 0)
				|| (((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) && (flag_recurse == 0))
		){
			if (FindNextFile( hFind, &FindFileData ) == 0 )
				break;
			continue;
		}

		i = j = 0;
		while (old_name[i] != 0){
			// Replace 1 Unicode character to 5 ASCII characters (%XXXX).
			if (old_name[i] & 0xFF80){
				if (flag_locale == 1){
					v = check_char(old_name + i);
				} else {
					v = 0;
				}
				if (v == 1){
					new_name[j++] = old_name[i];
				} else {
					new_name[j++] = '%';
					new_name[j++] = value_to_char((old_name[i] & 0xF000) >> 12);
					new_name[j++] = value_to_char((old_name[i] & 0x0F00) >> 8);
					new_name[j++] = value_to_char((old_name[i] & 0x00F0) >> 4);
					new_name[j++] = value_to_char(old_name[i] & 0x000F);
				}
			} else if ((flag_space == 1) && (old_name[i] == ' ')){	// replace space to %0020
				new_name[j++] = '%';
				new_name[j++] = '0';
				new_name[j++] = '0';
				new_name[j++] = '2';
				new_name[j++] = '0';
			} else if (old_name[i] == '%'){	// replace % to %0025
				new_name[j++] = '%';
				// Check following wards of the %, whether it's valid %XXXX format or not.
				if (old_name[i + 1] == 0){
					v = -1;
				} else if (old_name[i + 2] == 0){
					v = -1;
				} else if (old_name[i + 3] == 0){
					v = -1;
				} else if (old_name[i + 4] == 0){
					v = -1;
				} else {
					v = char_to_value(old_name[i + 1]);
					v = (v << 4) | char_to_value(old_name[i + 2]);
					v = (v << 4) | char_to_value(old_name[i + 3]);
					v = (v << 4) | char_to_value(old_name[i + 4]);
				}
				if ((v & 0xFFFF0000) == 0){	// It's a valid %XXXX format.
					if (flag_force == 1){	// It inserts "0025", only when "/force" option is set.
						new_name[j++] = '0';
						new_name[j++] = '0';
						new_name[j++] = '2';
						new_name[j++] = '5';
					}
					i++;
					new_name[j++] = old_name[i];
					i++;
					new_name[j++] = old_name[i];
					i++;
					new_name[j++] = old_name[i];
					i++;
					new_name[j++] = old_name[i];
					// sign of valid %XXXX format
					if ((flag_force % 10) == 0)
						flag_force -= 2;
				}
			} else {
				new_name[j++] = old_name[i];
			}
			i++;
		}
		new_name[j] = 0;

		//printf_cp("%s\n", old_name);
		if (i != j){
			//printf_cp("%s -> ", old_name);
			//printf_cp("%s\n", new_name);
			if (j >= MAX_PATH){
				printf_cp("Error: new name '%s' is too long.\n", new_name);
				FindClose( hFind );
				return 1;
			}
			replace_count++;
		}

		// count of valid %XXXX format
		if ((flag_force % 10) == -2)
			flag_force -= 8;

		// sub-directory
		if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){
			i = wcslen(parent_dir) + wcslen(old_name) + 3;	// '\' +  '*' + 'null'
			child_dir = malloc(i * sizeof(wchar_t));
			if (child_dir != NULL){
				wcscpy(child_dir, parent_dir);
				wcscat(child_dir, old_name);
				wcscat(child_dir, L"\\");
				i = search_dir_check(child_dir);
				free(child_dir);
				if (i != 0){
					FindClose( hFind );
					return i;
				}
				//printf_cp("directory = %s\n", parent_dir);
			} else {
				printf("Error: failed to go into sub-directory.\n");
			}
		}

		// search next matching file
		if (FindNextFile( hFind, &FindFileData ) == 0 )
			break;
	}
	FindClose( hFind );

	return 0;
}

int search_dir_change(wchar_t *parent_dir)
{
	int i, j, v;
	wchar_t new_name[MAX_PATH * 5], *old_name, *child_dir, *old_path, *new_path, buf[8];
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind;

	// Find first file in current directory
	i = wcslen(parent_dir);
	wcscpy(parent_dir + i, L"*");
	hFind = FindFirstFile(parent_dir, &FindFileData);
	parent_dir[i] = 0;
	if (hFind == INVALID_HANDLE_VALUE)
		return 0;
	while (hFind != INVALID_HANDLE_VALUE){
		old_name = FindFileData.cFileName;

		// Exclude this and paretnt directories
		if ((wcscmp(old_name, L".") == 0) || (wcscmp(old_name, L"..") == 0)
				|| ((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0)
				|| ((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0)
				|| ((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) != 0)
				|| (((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) && (flag_recurse == 0))
		){
			if (FindNextFile( hFind, &FindFileData ) == 0 )
				break;
			continue;
		}

		i = j = 0;
		while (old_name[i] != 0){
			// Replace 1 Unicode character to 5 ASCII characters (%XXXX).
			if (old_name[i] & 0xFF80){
				if (flag_locale == 1){
					v = check_char(old_name + i);
				} else {
					v = 0;
				}
				if (v == 1){
					new_name[j++] = old_name[i];
				} else {
					new_name[j++] = '%';
					new_name[j++] = value_to_char((old_name[i] & 0xF000) >> 12);
					new_name[j++] = value_to_char((old_name[i] & 0x0F00) >> 8);
					new_name[j++] = value_to_char((old_name[i] & 0x00F0) >> 4);
					new_name[j++] = value_to_char(old_name[i] & 0x000F);
				}
			} else if ((flag_space == 1) && (old_name[i] == ' ')){	// space
				new_name[j++] = '%';
				new_name[j++] = '0';
				new_name[j++] = '0';
				new_name[j++] = '2';
				new_name[j++] = '0';
			} else if ((flag_force == 1) && (old_name[i] == '%')){	// replace % to %0025
				new_name[j++] = '%';
				// Check following wards of the %, whether it's valid %XXXX format or not.
				if (old_name[i + 1] == 0){
					v = -1;
				} else if (old_name[i + 2] == 0){
					v = -1;
				} else if (old_name[i + 3] == 0){
					v = -1;
				} else if (old_name[i + 4] == 0){
					v = -1;
				} else {
					v = char_to_value(old_name[i + 1]);
					v = (v << 4) | char_to_value(old_name[i + 2]);
					v = (v << 4) | char_to_value(old_name[i + 3]);
					v = (v << 4) | char_to_value(old_name[i + 4]);
				}
				if ((v & 0xFFFF0000) == 0){	// It's a valid %XXXX format.
					new_name[j++] = '0';
					new_name[j++] = '0';
					new_name[j++] = '2';
					new_name[j++] = '5';
					i++;
					new_name[j++] = old_name[i];
					i++;
					new_name[j++] = old_name[i];
					i++;
					new_name[j++] = old_name[i];
					i++;
					new_name[j++] = old_name[i];
				}
			} else {
				new_name[j++] = old_name[i];
			}
			i++;
		}
		new_name[j] = 0;

		//printf_cp("%s\n", old_name);
		if (i != j){
			if (flag_yes == 0){	// Ask user's confirmation
				printf_cp("Are you sure you want to rename ?\nDir: %s\n", parent_dir + 4);
				printf_cp("Old: %s\n", old_name);
				printf_cp("New: %s\n", new_name);
				do {
					printf("Type Y for Yes, A for All, N for No, or Q to Quit: ");
					v = _cgetws_s(buf, 8, &i);
					if (v != 0){
						v = -1;
					} else if (i < 8){
						if ((_wcsicmp(buf, L"Y") == 0) || (_wcsicmp(buf, L"Yes") == 0)){
							v = 1;
						} else if ((_wcsicmp(buf, L"A") == 0) || (_wcsicmp(buf, L"All") == 0)){
							v = 1;
							flag_yes = 2;
						} else if ((_wcsicmp(buf, L"N") == 0) || (_wcsicmp(buf, L"No") == 0)){
							v = 2;
						} else if ((_wcsicmp(buf, L"Q") == 0) || (_wcsicmp(buf, L"Quit") == 0)){
							v = -1;
						}
					}
				} while (v == 0);
				if (v == -1){	// Quit or error
					printf("Renaming was aborted.\n\n");
					FindClose( hFind );
					return 2;
				}
			} else {	// always Yes
				v = 1;
			}

			if (v == 1){	// Yes
				//printf_cp("%s -> ", old_name);
				//printf_cp("%s\n", new_name);
				if (j >= MAX_PATH){
					printf_cp("Error: new name '%s' is too long.\n", new_name);
					FindClose( hFind );
					return 1;
				}
				i = wcslen(parent_dir) + wcslen(new_name) + 1;
				old_path = malloc(i * 2 * sizeof(wchar_t));
				if (old_path != NULL){
					new_path = old_path + i;
					wcscpy(old_path, parent_dir);
					wcscat(old_path, old_name);
					wcscpy(new_path, parent_dir);
					wcscat(new_path, new_name);
					if (MoveFile(old_path, new_path) == 0){
						printf("Error %d: ", GetLastError());
						printf_cp("failed to rename '%s'.\n", old_path);
						free(old_path);
						FindClose( hFind );
						return 1;
					}
					free(old_path);
				} else {
					printf_cp("Error: names '%s' are too long.\n", old_name);
				}
				replace_count++;
				if (flag_yes == 0){
					printf("\n");
				} else if (flag_yes == 2){
					flag_yes = 1;
					printf("\n");
				}
			} else {	// No
				printf("Renaming was skipped.\n\n");
			}
		}

		// sub-directory
		if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){
			i = wcslen(parent_dir) + wcslen(new_name) + 3;	// '\' +  '*' + 'null'
			child_dir = malloc(i * sizeof(wchar_t));
			if (child_dir != NULL){
				wcscpy(child_dir, parent_dir);
				wcscat(child_dir, new_name);
				wcscat(child_dir, L"\\");
				i = search_dir_change(child_dir);
				free(child_dir);
				if (i != 0){
					FindClose( hFind );
					return i;
				}
				//printf_cp("directory = %s\n", parent_dir);
			} else {
				printf("Error: failed to go into sub-directory.\n");
			}
		}

		// search next matching file
		if (FindNextFile( hFind, &FindFileData ) == 0 )
			break;
	}
	FindClose( hFind );

	return 0;
}

int search_dir_return(wchar_t *parent_dir)
{
	int i, j, v;
	wchar_t new_name[MAX_PATH], *old_name, *child_dir, *old_path, *new_path, buf[8];
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind;

	// Find first file in current directory
	i = wcslen(parent_dir);
	wcscpy(parent_dir + i, L"*");
	hFind = FindFirstFile(parent_dir, &FindFileData);
	parent_dir[i] = 0;
	if (hFind == INVALID_HANDLE_VALUE)
		return 0;
	while (hFind != INVALID_HANDLE_VALUE){
		old_name = FindFileData.cFileName;

		// Exclude this and paretnt directories
		if ((wcscmp(old_name, L".") == 0) || (wcscmp(old_name, L"..") == 0)
				|| ((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0)
				|| ((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0)
				|| ((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) != 0)
				|| (((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) && (flag_recurse == 0))
		){
			if (FindNextFile( hFind, &FindFileData ) == 0 )
				break;
			continue;
		}

		i = j = 0;
		while (old_name[i] != 0){
			// Replace 5 ASCII characters (%XXXX) to 1 Unicode character.
			if (old_name[i] == '%'){
				if (old_name[i + 1] == 0){
					v = -1;
				} else if (old_name[i + 2] == 0){
					v = -1;
				} else if (old_name[i + 3] == 0){
					v = -1;
				} else if (old_name[i + 4] == 0){
					v = -1;
				} else {
					v = char_to_value(old_name[i + 1]);
					v = (v << 4) | char_to_value(old_name[i + 2]);
					v = (v << 4) | char_to_value(old_name[i + 3]);
					v = (v << 4) | char_to_value(old_name[i + 4]);
				}
				if (v & 0xFFFF0000){	// It did not replace.
					new_name[j] = '%';
				} else {
					new_name[j] = v;
					i += 4;
				}
			} else {
				new_name[j] = old_name[i];
			}
			i++;
			j++;
		}
		new_name[j] = 0;

		//printf_cp("%s\n", old_name);
		if (i != j){
			if (flag_yes == 0){	// Ask user's confirmation
				printf_cp("Are you sure you want to rename ?\nDir: %s\n", parent_dir + 4);
				printf_cp("Old: %s\n", old_name);
				printf_cp("New: %s\n", new_name);
				do {
					printf("Type Y for Yes, A for All, N for No, or Q to Quit: ");
					v = _cgetws_s(buf, 8, &i);
					if (v != 0){
						v = -1;
					} else if (i < 8){
						if ((_wcsicmp(buf, L"Y") == 0) || (_wcsicmp(buf, L"Yes") == 0)){
							v = 1;
						} else if ((_wcsicmp(buf, L"A") == 0) || (_wcsicmp(buf, L"All") == 0)){
							v = 1;
							flag_yes = 2;
						} else if ((_wcsicmp(buf, L"N") == 0) || (_wcsicmp(buf, L"No") == 0)){
							v = 2;
						} else if ((_wcsicmp(buf, L"Q") == 0) || (_wcsicmp(buf, L"Quit") == 0)){
							v = -1;
						}
					}
				} while (v == 0);
				if (v == -1){	// Quit or error
					printf("Renaming was aborted.\n\n");
					FindClose( hFind );
					return 2;
				}
			} else {	// always Yes
				v = 1;
			}

			if (v == 1){	// Yes
				//printf_cp("%s -> ", old_name);
				//printf_cp("%s\n", new_name);
				i = wcslen(parent_dir) + wcslen(old_name) + 1;
				old_path = malloc(i * 2 * sizeof(wchar_t));
				if (old_path != NULL){
					new_path = old_path + i;
					wcscpy(old_path, parent_dir);
					wcscat(old_path, old_name);
					wcscpy(new_path, parent_dir);
					wcscat(new_path, new_name);
					if (MoveFile(old_path, new_path) == 0){
						printf("Error %d: ", GetLastError());
						printf_cp("failed to rename '%s'.\n", old_path);
						free(old_path);
						FindClose( hFind );
						return 1;
					}
					free(old_path);
				} else {
					printf_cp("Error: names '%s' are too long.\n", old_name);
				}
				replace_count++;
				if (flag_yes == 0){
					printf("\n");
				} else if (flag_yes == 2){
					flag_yes = 1;
					printf("\n");
				}
			} else {	// No
				printf("Renaming was skipped.\n\n");
			}
		}

		// sub-directory
		if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){
			i = wcslen(parent_dir) + wcslen(new_name) + 3;	// '\' +  '*' + 'null'
			child_dir = malloc(i * sizeof(wchar_t));
			if (child_dir != NULL){
				wcscpy(child_dir, parent_dir);
				wcscat(child_dir, new_name);
				wcscat(child_dir, L"\\");
				i = search_dir_return(child_dir);
				free(child_dir);
				if (i != 0){
					FindClose( hFind );
					return i;
				}
				//printf_cp("directory = %s\n", parent_dir);
			} else {
				printf("Error: failed to go into sub-directory.\n");
			}
		}

		// search next matching file
		if (FindNextFile( hFind, &FindFileData ) == 0 )
			break;
	}
	FindClose( hFind );

	return 0;
}

static int check_filename(wchar_t *name, int len)
{
	// space at the first or last character
	// period at the last character
	if ((name[0] == ' ') || (name[len - 1] == ' ') || (name[len - 1] == '.'))
		return 1;

	// Reserved filenames on Windows OS
	// CON, PRN, AUX, NUL, COM1..COM9, LPT1..LPT9
	if (len >= 3){
		if ((name[3] == 0) || (name[3] == '.')){
			if (_wcsnicmp(name, L"CON", 3) == 0)
				return 1;
			if (_wcsnicmp(name, L"PRN", 3) == 0)
				return 1;
			if (_wcsnicmp(name, L"AUX", 3) == 0)
				return 1;
			if (_wcsnicmp(name, L"NUL", 3) == 0)
				return 1;
		}
		if (len >= 4){
			if ((name[4] == 0) || (name[4] == '.')){
				if (_wcsnicmp(name, L"COM", 3) == 0){
					if ((name[3] >= '1') && (name[3] <= '9'))
						return 1;
				}
				if (_wcsnicmp(name, L"LPT", 3) == 0){
					if ((name[3] >= '1') && (name[3] <= '9'))
						return 1;
				}
			}
		}
	}

	return 0;
}

int search_dir_sanitize(wchar_t *parent_dir)
{
	int i, j, v, len;
	wchar_t new_name[MAX_PATH * 5], *old_name, *child_dir, *old_path, *new_path, buf[8];
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind;

	// Find first file in current directory
	i = wcslen(parent_dir);
	wcscpy(parent_dir + i, L"*");
	hFind = FindFirstFile(parent_dir, &FindFileData);
	parent_dir[i] = 0;
	if (hFind == INVALID_HANDLE_VALUE)
		return 0;
	while (hFind != INVALID_HANDLE_VALUE){
		old_name = FindFileData.cFileName;

		// Exclude this and paretnt directories
		if ((wcscmp(old_name, L".") == 0) || (wcscmp(old_name, L"..") == 0)
				|| ((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0)
				|| ((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0)
				|| ((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) != 0)
				|| (((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) && (flag_recurse == 0))
		){
			if (FindNextFile( hFind, &FindFileData ) == 0 )
				break;
			continue;
		}

		// Test invalid filename by trying to rename
		len = wcslen(old_name);
		if (check_filename(old_name, len) != 0){
			//printf_cp("\"%s\" is invalid\n", old_name);
			i = j = 0;
			while (old_name[i] != 0){
/*
				// Replace 1 Unicode character to 5 ASCII characters (%XXXX).
				if (old_name[i] & 0xFF80){
					if (flag_locale == 1){
						v = check_char(old_name + i);
					} else {
						v = 0;
					}
					if (v == 1){
						new_name[j++] = old_name[i];
					} else {
						new_name[j++] = '%';
						new_name[j++] = value_to_char((old_name[i] & 0xF000) >> 12);
						new_name[j++] = value_to_char((old_name[i] & 0x0F00) >> 8);
						new_name[j++] = value_to_char((old_name[i] & 0x00F0) >> 4);
						new_name[j++] = value_to_char(old_name[i] & 0x000F);
					}
				} else 
*/
				if ((i == len - 1) && (old_name[i] == '.')){	// period
					new_name[j++] = '%';
					new_name[j++] = '0';
					new_name[j++] = '0';
					new_name[j++] = '2';
					new_name[j++] = 'E';
				} else if ((old_name[i] == ' ') && ((i == 0) || (i == len - 1) || (flag_space == 1))){	// space
					new_name[j++] = '%';
					new_name[j++] = '0';
					new_name[j++] = '0';
					new_name[j++] = '2';
					new_name[j++] = '0';
				} else if ((i == 0) && (_wcsnicmp(old_name, L"CON", 3) == 0)
						&& ((old_name[3] == 0) || (old_name[3] == '.'))){	// CON , CON.*
					new_name[j++] = '%';
					new_name[j++] = '0';
					new_name[j++] = '0';
					if (old_name[0] == 'C'){
						new_name[j++] = '4';
					} else {
						new_name[j++] = '6';
					}
					new_name[j++] = '3';
				} else if ((i == 0) && (_wcsnicmp(old_name, L"PRN", 3) == 0)
						&& ((old_name[3] == 0) || (old_name[3] == '.'))){	// PRN , PRN.*
					new_name[j++] = '%';
					new_name[j++] = '0';
					new_name[j++] = '0';
					if (old_name[0] == 'P'){
						new_name[j++] = '5';
					} else {
						new_name[j++] = '7';
					}
					new_name[j++] = '0';
				} else if ((i == 0) && (_wcsnicmp(old_name, L"AUX", 3) == 0)
						&& ((old_name[3] == 0) || (old_name[3] == '.'))){	// AUX , AUX.*
					new_name[j++] = '%';
					new_name[j++] = '0';
					new_name[j++] = '0';
					if (old_name[0] == 'A'){
						new_name[j++] = '4';
					} else {
						new_name[j++] = '6';
					}
					new_name[j++] = '1';
				} else if ((i == 0) && (_wcsnicmp(old_name, L"NUL", 3) == 0)
						&& ((old_name[3] == 0) || (old_name[3] == '.'))){	// NUL , NUL.*
					new_name[j++] = '%';
					new_name[j++] = '0';
					new_name[j++] = '0';
					if (old_name[0] == 'N'){
						new_name[j++] = '4';
					} else {
						new_name[j++] = '6';
					}
					new_name[j++] = 'E';
				} else if ((i == 0) && (_wcsnicmp(old_name, L"COM", 3) == 0)
						&& ((old_name[3] >= '1') && (old_name[3] <= '9'))
						&& ((old_name[4] == 0) || (old_name[4] == '.'))){	// COM# , COM#.*
					new_name[j++] = '%';
					new_name[j++] = '0';
					new_name[j++] = '0';
					if (old_name[0] == 'C'){
						new_name[j++] = '4';
					} else {
						new_name[j++] = '6';
					}
					new_name[j++] = '3';
				} else if ((i == 0) && (_wcsnicmp(old_name, L"LPT", 3) == 0)
						&& ((old_name[3] >= '1') && (old_name[3] <= '9'))
						&& ((old_name[4] == 0) || (old_name[4] == '.'))){	// LPT# , LPT#.*
					new_name[j++] = '%';
					new_name[j++] = '0';
					new_name[j++] = '0';
					if (old_name[0] == 'L'){
						new_name[j++] = '4';
					} else {
						new_name[j++] = '6';
					}
					new_name[j++] = 'C';
				} else if ((flag_force == 1) && (old_name[i] == '%')){	// replace % to %0025
					new_name[j++] = '%';
					// Check following wards of the %, whether it's valid %XXXX format or not.
					if (old_name[i + 1] == 0){
						v = -1;
					} else if (old_name[i + 2] == 0){
						v = -1;
					} else if (old_name[i + 3] == 0){
						v = -1;
					} else if (old_name[i + 4] == 0){
						v = -1;
					} else {
						v = char_to_value(old_name[i + 1]);
						v = (v << 4) | char_to_value(old_name[i + 2]);
						v = (v << 4) | char_to_value(old_name[i + 3]);
						v = (v << 4) | char_to_value(old_name[i + 4]);
					}
					if ((v & 0xFFFF0000) == 0){	// It's a valid %XXXX format.
						new_name[j++] = '0';
						new_name[j++] = '0';
						new_name[j++] = '2';
						new_name[j++] = '5';
						i++;
						new_name[j++] = old_name[i];
						i++;
						new_name[j++] = old_name[i];
						i++;
						new_name[j++] = old_name[i];
						i++;
						new_name[j++] = old_name[i];
					}
				} else {
					new_name[j++] = old_name[i];
				}
				i++;
			}
			new_name[j] = 0;

			//printf_cp("%s\n", old_name);
			if (i != j){
				if (flag_yes == 0){	// Ask user's confirmation
					printf_cp("Are you sure you want to rename ?\nDir: %s\n", parent_dir + 4);
					printf_cp("Old: %s\n", old_name);
					printf_cp("New: %s\n", new_name);
					do {
						printf("Type Y for Yes, A for All, N for No, or Q to Quit: ");
						v = _cgetws_s(buf, 8, &i);
						if (v != 0){
							v = -1;
						} else if (i < 8){
							if ((_wcsicmp(buf, L"Y") == 0) || (_wcsicmp(buf, L"Yes") == 0)){
								v = 1;
							} else if ((_wcsicmp(buf, L"A") == 0) || (_wcsicmp(buf, L"All") == 0)){
								v = 1;
								flag_yes = 2;
							} else if ((_wcsicmp(buf, L"N") == 0) || (_wcsicmp(buf, L"No") == 0)){
								v = 2;
							} else if ((_wcsicmp(buf, L"Q") == 0) || (_wcsicmp(buf, L"Quit") == 0)){
								v = -1;
							}
						}
					} while (v == 0);
					if (v == -1){	// Quit or error
						printf("Renaming was aborted.\n\n");
						FindClose( hFind );
						return 2;
					}
				} else {	// always Yes
					v = 1;
				}

				if (v == 1){	// Yes
					//printf_cp("%s -> ", old_name);
					//printf_cp("%s\n", new_name);
					if (j >= MAX_PATH){
						printf_cp("Error: new name '%s' is too long.\n", new_name);
						FindClose( hFind );
						return 1;
					}
					i = wcslen(parent_dir) + wcslen(new_name) + 1;
					old_path = malloc(i * 2 * sizeof(wchar_t));
					if (old_path != NULL){
						new_path = old_path + i;
						wcscpy(old_path, parent_dir);
						wcscat(old_path, old_name);
						wcscpy(new_path, parent_dir);
						wcscat(new_path, new_name);
						if (MoveFile(old_path, new_path) == 0){
							printf("Error %d: ", GetLastError());
							printf_cp("failed to rename '%s'.\n", old_path);
							free(old_path);
							FindClose( hFind );
							return 1;
						}
						free(old_path);
					} else {
						printf_cp("Error: names '%s' are too long.\n", old_name);
					}
					replace_count++;
					if (flag_yes == 0){
						printf("\n");
					} else if (flag_yes == 2){
						flag_yes = 1;
						printf("\n");
					}
				} else {	// No
					printf("Renaming was skipped.\n\n");
				}
			}
		}

		// sub-directory
		if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){
			i = wcslen(parent_dir) + wcslen(new_name) + 3;	// '\' +  '*' + 'null'
			child_dir = malloc(i * sizeof(wchar_t));
			if (child_dir != NULL){
				wcscpy(child_dir, parent_dir);
				wcscat(child_dir, new_name);
				wcscat(child_dir, L"\\");
				i = search_dir_change(child_dir);
				free(child_dir);
				if (i != 0){
					FindClose( hFind );
					return i;
				}
				//printf_cp("directory = %s\n", parent_dir);
			} else {
				printf("Error: failed to go into sub-directory.\n");
			}
		}

		// search next matching file
		if (FindNextFile( hFind, &FindFileData ) == 0 )
			break;
	}
	FindClose( hFind );

	return 0;
}

int wmain(int argc, wchar_t *argv[])
{
	int i, ret;
	int flag_replace = 0;
	wchar_t *cur_dir, *tmp_p;

	printf("Rename to 7-bit ASCII by (c) 2022 Yutaka Sawada\n\n");

	cur_dir = malloc(2048 * sizeof(wchar_t));	// maybe 2k length is enough.
	if (cur_dir == NULL){
		printf("Error: failed to get current directory.\n");
		return 1;
	}
	wcscpy(cur_dir, L"\\\\?\\");
	if (GetCurrentDirectory(2040, cur_dir + 4) == 0){
		free(cur_dir);
		printf("Error: failed to get current directory.\n");
		return 1;
	}
	wcscat(cur_dir, L"\\");

	for (i = 1; i < argc; i++){
		tmp_p = argv[i];
		if ((tmp_p[0] != '/') && (tmp_p[0] != '-')){
			printf("\"%S\" is invalid.\n", tmp_p);
			continue;
		}
		tmp_p++;

		if ((_wcsicmp(tmp_p, L"e") == 0) || (_wcsicmp(tmp_p, L"encode") == 0)){
			flag_replace = 1;
		} else if ((_wcsicmp(tmp_p, L"d") == 0) || (_wcsicmp(tmp_p, L"decode") == 0)){
			flag_replace = 2;
		} else if ((_wcsicmp(tmp_p, L"w") == 0) || (_wcsicmp(tmp_p, L"windows") == 0)){
			flag_replace = 4;
		} else if ((_wcsicmp(tmp_p, L"y") == 0) || (_wcsicmp(tmp_p, L"yes") == 0)){
			flag_yes = 1;
		} else if ((_wcsicmp(tmp_p, L"s") == 0) || (_wcsicmp(tmp_p, L"space") == 0)){
			flag_space = 1;
		} else if ((_wcsicmp(tmp_p, L"f") == 0) || (_wcsicmp(tmp_p, L"force") == 0)){
			flag_force = 1;
		} else if ((_wcsicmp(tmp_p, L"r") == 0) || (_wcsicmp(tmp_p, L"recurse") == 0)){
			flag_recurse = 1;
		} else if ((_wcsicmp(tmp_p, L"l") == 0) || (_wcsicmp(tmp_p, L"locale") == 0)){
			flag_locale = 1;
		} else if ((_wcsicmp(tmp_p, L"u") == 0) || (_wcsicmp(tmp_p, L"unicode") == 0)){
			cp_output = CP_UTF8;
		} else {
			printf("\"%S\" is invalid.\n", tmp_p - 1);
		}
	}

	// get console output Code Page
	cp_original = GetConsoleOutputCP();	// return 0 at error; same as CP_ACP
	if (cp_output != 0){
		// Try to change console output Code Page to UTF-8
		if (SetConsoleOutputCP(CP_UTF8) == 0){
			cp_output = cp_original;
			if (cp_original != 0){	// If it fails, it returns to the original Code page.
				SetConsoleOutputCP(cp_original);

				// get current console output Code Page again
				cp_output = GetConsoleOutputCP();
			}
		}
	} else {
		cp_output = cp_original;
	}
	//printf("cp_original = %d, cp_output = %d\n", cp_original, cp_output);

	if (flag_replace == 1){
		ret = search_dir_change(cur_dir);
		if (flag_recurse == 0){
			if (replace_count == 1){
				printf("Total %d file was renamed.\n", replace_count);
			} else {
				printf("Total %d files were renamed.\n", replace_count);
			}
		} else {
			if (replace_count == 1){
				printf("Total %d file or folder was renamed.\n", replace_count);
			} else {
				printf("Total %d files and/or folders were renamed.\n", replace_count);
			}
		}
	} else if (flag_replace == 2){
		ret = search_dir_return(cur_dir);
		if (flag_recurse == 0){
			if (replace_count == 1){
				printf("Total %d file was restored.\n", replace_count);
			} else {
				printf("Total %d files were restored.\n", replace_count);
			}
		} else {
			if (replace_count == 1){
				printf("Total %d file or folder was restored.\n", replace_count);
			} else {
				printf("Total %d files and/or folders were restored.\n", replace_count);
			}
		}
	} else if (flag_replace == 4){
		ret = search_dir_sanitize(cur_dir);
		if (flag_recurse == 0){
			if (replace_count == 1){
				printf("Total %d file was renamed.\n", replace_count);
			} else {
				printf("Total %d files were renamed.\n", replace_count);
			}
		} else {
			if (replace_count == 1){
				printf("Total %d file or folder was renamed.\n", replace_count);
			} else {
				printf("Total %d files and/or folders were renamed.\n", replace_count);
			}
		}
	} else {
		printf("This replaces Unicode to %%XXXX in filenames under current directory.\n\nOptions:\n"
				"/e or /encode  : rename filenames by replacing Unicode to %%XXXX\n"
				"/d or /decode  : restore original filenames with Unicode\n"
				"/y or /yes     : rename automatically\n"
				"/r or /recurse : search sub-directories recursively\n"
				"/s or /space   : substitute each space to %%0020\n"
				"/f or /force   : substitute each valid %%XXXX to %%0025XXXX\n"
				"/l or /locale  : replace only invalid characters at the locale\n"
				"/w or /windows : replace only invalid filenames on Windows OS\n"
				"/u or /unicode : try to display Unicode characters\n\n");
		ret = search_dir_check(cur_dir);
		if (flag_recurse == 0){
			if (replace_count == 1){
				printf("Total %d file will be renamed.\n", replace_count);
			} else {
				printf("Total %d files will be renamed.\n", replace_count);
			}
		} else {
			if (replace_count == 1){
				printf("Total %d file or folder will be renamed.\n", replace_count);
			} else {
				printf("Total %d files and/or folders will be renamed.\n", replace_count);
			}
		}
		if ((flag_force < 0) && (replace_count > 0)){
			if (flag_force / -10 == 1){
				printf("\nWarning: %d file already has %%XXXX signs.\n", flag_force / -10);
			} else {
				printf("\nWarning: %d files already have %%XXXX signs.\n", flag_force / -10);
			}
			printf("To prevent faulty restoring, use \"/force\" switch.\n");
		}
	}

	free(cur_dir);

	// Return to original Code Page
	if (cp_original != cp_output)
		SetConsoleOutputCP(cp_original);

	return ret;
}

