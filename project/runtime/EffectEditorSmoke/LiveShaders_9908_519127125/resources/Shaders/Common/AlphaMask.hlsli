#ifndef KOHAKU_ALPHA_MASK
#define KOHAKU_ALPHA_MASK
// Zero disables masking; equality with the cutoff remains visible.
bool ShouldRejectAlpha(float textureAlpha, float materialAlpha, float alphaCutoff) {
    return alphaCutoff > 0.0f && textureAlpha * materialAlpha < alphaCutoff;
}
#endif
