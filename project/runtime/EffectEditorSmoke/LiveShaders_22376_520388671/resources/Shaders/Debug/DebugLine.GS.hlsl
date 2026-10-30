struct GeometryShaderInput
{
    float32_t4 position : SV_POSITION;
    float32_t4 color : COLOR0;
    float32_t thickness : THICKNESS0;
};

struct GeometryShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t4 color : COLOR0;
};

struct ScreenData
{
    float32_t2 screenSize;
    float32_t2 padding;
};

ConstantBuffer<ScreenData> gScreenData : register(b1);

[maxvertexcount(4)]
void main(line GeometryShaderInput input[2], inout TriangleStream<GeometryShaderOutput> outputStream)
{
    if (input[0].position.w <= 0.0f) {
        return;
    }

    if (input[1].position.w <= 0.0f) {
        return;
    }

    float32_t2 ndcStart = input[0].position.xy / input[0].position.w;
    float32_t2 ndcEnd = input[1].position.xy / input[1].position.w;
    float32_t2 direction = ndcEnd - ndcStart;
    float32_t directionLength = length(direction);

    if (directionLength <= 0.00001f) {
        return;
    }

    direction /= directionLength;

    float32_t2 normal = float32_t2(-direction.y, direction.x);

    float32_t thickness = input[0].thickness;
    if (input[1].thickness > thickness) {
        thickness = input[1].thickness;
    }

    if (thickness < 1.0f) {
        thickness = 1.0f;
    }

    float32_t halfThickness = thickness * 0.5f;
    float32_t2 pixelToNdc = float32_t2(2.0f / gScreenData.screenSize.x, 2.0f / gScreenData.screenSize.y);
    float32_t2 offsetNdc = normal * halfThickness * pixelToNdc;

    GeometryShaderOutput output;

    output.color = input[0].color;
    output.position = input[0].position;
    output.position.xy += offsetNdc * output.position.w;
    outputStream.Append(output);

    output.color = input[0].color;
    output.position = input[0].position;
    output.position.xy -= offsetNdc * output.position.w;
    outputStream.Append(output);

    output.color = input[1].color;
    output.position = input[1].position;
    output.position.xy += offsetNdc * output.position.w;
    outputStream.Append(output);

    output.color = input[1].color;
    output.position = input[1].position;
    output.position.xy -= offsetNdc * output.position.w;
    outputStream.Append(output);
}
