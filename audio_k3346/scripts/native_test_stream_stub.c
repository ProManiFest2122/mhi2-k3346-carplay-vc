/* Emulator-only test stub standing in for libairplay's AudioStreamPrepare/Stop behind the hook (RTLD_NEXT).
 * Never part of any package. */
int AudioStreamPrepare(void *stream) { (void)stream; return 0; }
void AudioStreamStop(void *stream, unsigned char drain) { (void)stream; (void)drain; }
