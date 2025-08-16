#pragma once
#include <Device/Protocol/ProtocolSettingsWidget.hpp>
#include "LSLSpecificSettings.hpp"

class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;
class QSpinBox;
class QDoubleSpinBox;
class QComboBox;

namespace Protocols
{

class LSLProtocolSettingsWidget final : public Device::ProtocolSettingsWidget
{
public:
  explicit LSLProtocolSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

private:
  void on_addSensor();
  void on_removeSensor();
  void on_addChannel();
  void on_removeChannel();
  void on_itemChanged(QTreeWidgetItem* item, int column);
  void on_itemDoubleClicked(QTreeWidgetItem* item, int column);
  void updateOutboundButtons();

private:
  void populateInboundTree();
  void populateOutboundTree();
  
  // UI elements
  QLineEdit* m_name;
  QLineEdit* m_streamTypeFilter;
  QTreeWidget* m_inboundTree;
  QTreeWidget* m_outboundTree;
  
  QPushButton* m_addSensorBtn;
  QPushButton* m_removeSensorBtn;
  QPushButton* m_addChannelBtn;
  QPushButton* m_removeChannelBtn;
  
  // Settings
  LSLSpecificSettings m_settings;
};
}
