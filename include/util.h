#pragma once

#define concat_(a, b) a##b
#define concat(a, b) concat_(a, b)

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
