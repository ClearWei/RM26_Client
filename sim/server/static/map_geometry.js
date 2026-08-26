(function (root, factory) {
    const api = factory();

    if (typeof module === 'object' && module.exports) {
        module.exports = api;
    }

    root.MapGeometry = api;
}(typeof globalThis !== 'undefined' ? globalThis : this, function () {
    'use strict';

    function toFiniteNumber(value, fallback) {
        const number = Number(value);
        return Number.isFinite(number) ? number : fallback;
    }

    /**
     * 将浏览器视口坐标换算为 canvas 后备缓冲区像素坐标。
     * CSS 调整画布尺寸时必须应用缩放比例，例如全屏或高 DPI 显示场景。
     */
    function clientPointToCanvas(clientX, clientY, rect, canvasWidth, canvasHeight) {
        const left = toFiniteNumber(rect && rect.left, 0);
        const top = toFiniteNumber(rect && rect.top, 0);
        const cssWidth = toFiniteNumber(rect && rect.width, 0);
        const cssHeight = toFiniteNumber(rect && rect.height, 0);
        const width = Math.max(0, toFiniteNumber(canvasWidth, 0));
        const height = Math.max(0, toFiniteNumber(canvasHeight, 0));

        if (cssWidth <= 0 || cssHeight <= 0 || width <= 0 || height <= 0) {
            return { x: 0, y: 0 };
        }

        return {
            x: (toFiniteNumber(clientX, left) - left) * width / cssWidth,
            y: (toFiniteNumber(clientY, top) - top) * height / cssHeight
        };
    }

    return {
        clientPointToCanvas
    };
}));
