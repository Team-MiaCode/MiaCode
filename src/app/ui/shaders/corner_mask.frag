#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 baseColor;
    vec4 surfaceColor;
    float nativeMaterial;
    vec4 nativeTintColor;
    vec4 wallpaperBaseColor;
    vec4 panelBackingColor;
    vec4 separatorColor;
    float separatorWidth;
};
layout(binding = 1) uniform sampler2D source;
layout(binding = 2) uniform sampler2D panelSource;

void main()
{
    vec4 wallpaper = texture(source, qt_TexCoord0);
    vec4 sceneBacking = wallpaper + wallpaperBaseColor * (1.0 - wallpaper.a);
    vec4 backing = sceneBacking;
    backing = surfaceColor + backing * (1.0 - surfaceColor.a);

    float distanceToCenter = length(qt_TexCoord0 - vec2(1.0));
    float edgeWidth = length(vec2(dFdx(distanceToCenter), dFdy(distanceToCenter)));
    float coverage = smoothstep(1.0 - edgeWidth * 0.5,
                               1.0 + edgeWidth * 0.5, distanceToCenter);
    vec4 panel = texture(panelSource, qt_TexCoord0);
    panel += panelBackingColor * (1.0 - panel.a);
    if (nativeMaterial > 0.5) {
        panel += nativeTintColor * (1.0 - panel.a);
        fragColor = mix(panel, nativeTintColor, coverage) * qt_Opacity;
    } else {
        panel += sceneBacking * (1.0 - panel.a);
        fragColor = mix(panel, backing, coverage) * qt_Opacity;
    }
    float innerCoverage = smoothstep(1.0 - separatorWidth - edgeWidth * 0.5,
                                    1.0 - separatorWidth + edgeWidth * 0.5,
                                    distanceToCenter);
    float separatorCoverage = clamp(innerCoverage - coverage, 0.0, 1.0);
    vec4 separator = separatorColor * separatorCoverage * qt_Opacity;
    fragColor = separator + fragColor * (1.0 - separator.a);
}
