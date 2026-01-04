//The random number generation is based on code available at: https://github.com/Zydak/Vulkan-Path-Tracer

/*
MIT License

Copyright (c) 2023 Zydak

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

import Defines;

public uint Hash(uint v)
{
    v ^= v >> 16;
    v *= 0x7feb352du;
    v ^= v >> 15;
    v *= 0x846ca68bu;
    v ^= v >> 16;
    return v;
}

public struct Sampler
{
    uint m_Seed;

    public __init(uint seed) { m_Seed = seed; }

    [mutating]
    public uint PCG()
    {
        uint state = m_Seed * 747796405u + 2891336453u;
        uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
        m_Seed = (word >> 22u) ^ word;

        return m_Seed;
    }

    [mutating]
    public float UniformFloat()
    {
        uint hash = PCG();

        return float(hash) / float(UINT_MAX);
    }

    [mutating]
    public float2 UniformFloat2()
    {
        uint hash = PCG();
        float x1 = float(hash) / float(UINT_MAX);
        hash = PCG();
        float x2 = float(hash) / float(UINT_MAX);

        return float2(x1, x2);
    }

    [mutating]
    public float3 UniformFloat3()
    {
        uint hash = PCG();
        float x1 = float(hash) / float(UINT_MAX);
        hash = PCG();
        float x2 = float(hash) / float(UINT_MAX);
        hash = PCG();
        float x3 = float(hash) / float(UINT_MAX);

        return float3(x1, x2, x3);
    }
}