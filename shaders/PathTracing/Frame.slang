//A orthonormal geometry frame
public struct Frame {
    float3 n;
    float3 t;
    float3 s;

    /*
    public __init(float3 n_in) {
        n = normalize(n_in);
        t = normalize(cross(n, float3(0.0, 0.0, 1.0)));
        s = normalize(cross(n, t));
    }
        */

    public __init(float3 n_in)
    {
        n = normalize(n_in);

        float sign = copysign(1.0, n.z);
        float a = -1.0 / (sign + n.z);
        float b = n.x * n.y * a;

        t = float3(1.0 + sign * n.x * n.x * a,
                   sign * b,
                   -sign * n.x);

        s = float3(b,
                   sign + n.y * n.y * a,
                   -n.y);
    }

    



    public __init(float3 n_in, float3 t_in) {
        n = normalize(n_in);
        t = normalize(t_in);
        s = cross(n, t);
    }

    /// Convert from world coordinates to local coordinates
    public float3 toLocal(in float3 v) {
        return float3(dot(v, s), dot(v, t), dot(v, n));
    }

    /// Convert from local coordinates to world coordinates
    public float3 toWorld(in float3 v) {
        return s * v.x + t * v.y + n * v.z;
    }
}