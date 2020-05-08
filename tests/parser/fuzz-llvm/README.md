This is an [LLVM LibFuzzer](https://llvm.org/docs/LibFuzzer.html)-based test.

A typical setup could look like this:

```
cd libstud-json
bdep init -C @fuzz cc config.cxx=clang++ config.cxx.coptions="-g -O3 -fsanitize=address,undefined,fuzzer-no-link"
b ../libstud-json-fuzz/libstud-json/tests/parser/fuzz-llvm/ # Directory may not exist at this point
cd ../libstud-json-fuzz/libstud-json/tests/parser/fuzz-llvm/
mkdir corpus
./driver corpus/
```

It is, however, highly recommended to pre-initialize the corpus with as many
samples (both valid and invalid) as possible. The following repositories are a
good starting point:
  * The `test_parsing/` directory from
    [JSONTestSuite](https://github.com/nst/JSONTestSuite)
  * The `json/corpus/` directory from
    [go-fuzz-corpus](https://github.com/dvyukov/go-fuzz-corpus/tree/master)
