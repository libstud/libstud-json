This is an [LLVM LibFuzzer](https://llvm.org/docs/LibFuzzer.html)-based test.

A typical setup could look like this:

```
cd libstud-json
bdep init -C @fuzz cc config.cxx=clang++ config.cxx.coptions="-g -O3 -fsanitize=address,undefined,fuzzer-no-link"
b ../libstud-json-fuzz/tests/fuzz-llvm/ # Directory may not exist at this point
cd ../libstud-json-fuzz/tests/fuzz-llvm/
mkdir corpus
./driver corpus/
```

It is, however, recommended to pre-initialize the corpus with some samples
(both valid and invalid). For example, you could use the `test_parsing/`
directory from [JSONTestSuite](https://github.com/nst/JSONTestSuite) as
a starting corpus directory.
