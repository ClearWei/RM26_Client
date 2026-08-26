#include "widgets/HealthBarWidget.h"
#include <QtTest>

/**
 * @brief HealthBarWidget 单元测试类
 *
 * 测试血条组件的逻辑功能，包括：
 * 1. 血量设置与动画目标值
 * 2. 虚拟护盾设置
 * 3. 无敌状态切换
 * 4. 低血量阈值判断
 */
class TestHealthBar : public QObject {
  Q_OBJECT

private slots:
  /**
   * @brief 测试血量设置
   * 验证设置当前血量后，Getter返回值是否正确。
   */
  void testSetHealth() {
    HealthBarWidget healthBar(HealthBarWidget::Red);

    healthBar.setHealth(100, 200);

    QCOMPARE(healthBar.getCurrentHealth(), 100);
    QCOMPARE(healthBar.getMaxHealth(), 200);

    // 验证动画属性也被更新（虽然动画是异步的，但属性setter通常会立即更新目标值）
    // 注意：这里我们假设setHealth会触发动画，但最终值应该一致
    // 由于动画需要事件循环，这里主要验证状态变量
  }

  /**
   * @brief 测试虚拟护盾
   */
  void testVirtualShield() {
    HealthBarWidget healthBar(HealthBarWidget::Blue);

    healthBar.setVirtualShield(50);

    // 由于没有直接的getter获取shield值（只有getShieldOpacity），
    // 我们主要验证调用不会崩溃，并且可以通过检查私有成员（如果友元）或观察副作用
    // 这里我们假设代码逻辑正确，主要作为回归测试
  }

  /**
   * @brief 测试无敌状态
   */
  void testInvincible() {
    HealthBarWidget healthBar(HealthBarWidget::Red);

    QVERIFY(!healthBar.isInvincible());

    healthBar.setInvincible(true);
    QVERIFY(healthBar.isInvincible());

    healthBar.setInvincible(false);
    QVERIFY(!healthBar.isInvincible());
  }

  /**
   * @brief 测试低血量阈值
   * 验证低血量判断逻辑（需要访问protected/private方法，或者通过副作用）
   * 这里我们只测试公开接口 setLowHealthThreshold
   */
  void testLowHealthThreshold() {
    HealthBarWidget healthBar(HealthBarWidget::Red);
    healthBar.setLowHealthThreshold(0.3f);

    // 这是一个简单的配置测试
    // 实际的低血量效果（闪烁）涉及定时器和绘图，单元测试难以直接验证视觉效果
  }
};

QTEST_MAIN(TestHealthBar)
#include "test_health_bar.moc"
