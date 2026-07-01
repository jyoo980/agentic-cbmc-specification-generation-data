import tools.run_cbmc as r
res=r.run_cbmc('ZopfliCacheToSublen','/app/Syzygy_Zopfli/c_code/zopfli.c',include_dirs=['/app/Syzygy_Zopfli/c_code'])
print('verified:', res.is_function_verified, '|', res.response[:120])
