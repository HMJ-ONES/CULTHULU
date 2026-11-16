// Original texture-free living obsidian. Reconstruct a continuous sphere from
// the stock sphere UVs so the pattern closes at the seam and converges at poles.
float longitude = UV.x * 6.2831853;
float latitude = UV.y * 3.14159265;
float3 p = float3(cos(longitude) * sin(latitude), sin(longitude) * sin(latitude), cos(latitude));
float a = sin(p.x * 13.0 + sin(p.z * 7.0) * 1.5);
float b = sin(p.y * 11.0 + sin(p.x * 8.0) * 1.2 + p.z * 4.0);
float field = a * b;
float width = max(fwidth(field) * 1.15, 0.018);
float vein = 1.0 - smoothstep(0.025, 0.025 + width, abs(field));
float grain = 0.5 + 0.5 * sin(p.x * 41.0 + sin(p.y * 33.0) + p.z * 37.0);
float variation = 0.82 + 0.18 * grain;
float3 surface = BodyColor * variation;
return surface * (1.0 - vein * 0.12) + VeinColor * (vein * 0.025);
