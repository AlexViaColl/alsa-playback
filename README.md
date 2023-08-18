# Playing a Sound with ALSA

An introduction to sound playback programming using [ALSA](https://www.alsa-project.org/).

Check out the accompanying [blog post](https://alexvia.com/post/003_alsa_playback/)
for a more in-depth explanation.

## Dependencies
- [gcc](https://gcc.gnu.org/)
- [pkg-config](https://www.freedesktop.org/wiki/Software/pkg-config/)
- [alsa-lib](https://www.alsa-project.org/wiki/Download)

## Quickstart
```console
$ ./build.sh
$ ./playback --raw skyhigh.raw --fade 3000
```

## References
- Elektronomia - Sky High: https://ncs.io/skyhigh
