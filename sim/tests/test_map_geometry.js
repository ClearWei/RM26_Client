'use strict';

const test = require('node:test');
const assert = require('node:assert/strict');
const { clientPointToCanvas } = require('../server/static/map_geometry.js');

test('将未缩放的客户端坐标换算为画布像素', () => {
    const point = clientPointToCanvas(
        410,
        245,
        { left: 10, top: 20, width: 800, height: 450 },
        800,
        450
    );

    assert.deepEqual(point, { x: 400, y: 225 });
});

test('兼容全屏 CSS 缩放和高 DPI 画布', () => {
    const point = clientPointToCanvas(
        510,
        301.25,
        { left: 110, top: 76.25, width: 1600, height: 900 },
        3200,
        1800
    );

    assert.deepEqual(point, { x: 800, y: 450 });
});

test('元素没有布局尺寸时返回安全原点', () => {
    const point = clientPointToCanvas(
        120,
        80,
        { left: 10, top: 10, width: 0, height: 0 },
        800,
        450
    );

    assert.deepEqual(point, { x: 0, y: 0 });
});
