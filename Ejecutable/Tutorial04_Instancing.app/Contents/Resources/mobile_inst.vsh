cbuffer Constants
{
    float4x4 g_ViewProj;
    float4x4 g_Rotation;
};

struct VSInput
{
    // Vertex attributes
    float3 Pos      : ATTRIB0;
    float2 UV       : ATTRIB1;

    // Instance attributes
    float4 MtrxRow0 : ATTRIB2;
    float4 MtrxRow1 : ATTRIB3;
    float4 MtrxRow2 : ATTRIB4;
    float4 MtrxRow3 : ATTRIB5;
};

struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 UV  : TEX_COORD;
};

void main(in  VSInput VSIn,
          out PSInput PSIn)
{
    float4x4 InstanceMatr = MatrixFromRows(VSIn.MtrxRow0, VSIn.MtrxRow1, VSIn.MtrxRow2, VSIn.MtrxRow3);
    
    // Aplicar rotación
    float4 TransformedPos = mul(float4(VSIn.Pos,1.0), g_Rotation);
    // Aplicar transformación específica de la instancia
    TransformedPos = mul(TransformedPos, InstanceMatr);
    // Aplicar matriz de vista-proyección
    PSIn.Pos = mul(TransformedPos, g_ViewProj);
    PSIn.UV  = VSIn.UV;
}
