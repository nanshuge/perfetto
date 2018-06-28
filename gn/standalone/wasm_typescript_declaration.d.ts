export = InitThisWasm;

declare function InitThisWasm(_: InitThisWasm.ModuleArgs): InitThisWasm.Module;

// See https://kripken.github.io/emscripten-site/docs/api_reference/module.html
declare namespace InitThisWasm {
  export interface InitWasm {
    (_: ModuleArgs): Module;
  }

  export interface Module {
    addFunction(f: any, argTypes: string): void;
    ccall(
        ident: string,
        returnType: string,
        argTypes: string[],
        args: any[],
    ): void;
    HEAPU8: Uint8Array;
  }

  export interface ModuleArgs {
    locateFile(s: string): string;
    print(s: string): void;
    printErr(s: string): void;
    onRuntimeInitialized(): void;
    onAbort(): void;
  }
}
