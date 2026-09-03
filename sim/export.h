#ifndef EXPORT_H
#define EXPORT_H
#ifdef __wasm__
#define SIM_EXPORT(name) __attribute__((export_name(name), visibility("default")))
#else
#define SIM_EXPORT(name)
#endif
#endif
