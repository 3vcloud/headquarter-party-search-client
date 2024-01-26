export function assert(boolFunc) {
    if(!boolFunc) {
        throw new Error("Assertion failure")
    }
}