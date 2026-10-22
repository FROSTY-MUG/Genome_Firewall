#include <pthread.h>
int main() { pthread_mutex_t m; pthread_mutex_init(&m, NULL); pthread_mutex_lock(&m); pthread_mutex_unlock(&m); return 0; }