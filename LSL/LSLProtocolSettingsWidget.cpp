#include "LSLProtocolSettingsWidget.hpp"

#include "LSLProtocolFactory.hpp"
#include "LSLSpecificSettings.hpp"

#include <ossia/detail/unique_instance.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <LSL/lsl_context.hpp>
#include <LSL/lsl_protocol.hpp>

namespace Protocols
{

LSLProtocolSettingsWidget::LSLProtocolSettingsWidget(QWidget* parent)
    : Device::ProtocolSettingsWidget(parent)
{
  // Main layout
  auto mainLayout = new QVBoxLayout;
  mainLayout->setContentsMargins(0, 0, 0, 0);
  setLayout(mainLayout);

  // Settings form
  auto settingsForm = new QFormLayout;
  
  m_name = new QLineEdit;
  checkForChanges(m_name);
  settingsForm->addRow(tr("Name:"), m_name);
  
  m_streamTypeFilter = new QLineEdit;
  m_streamTypeFilter->setPlaceholderText(tr("Leave empty for all types"));
  settingsForm->addRow(tr("Stream Types:"), m_streamTypeFilter);
  
  mainLayout->addLayout(settingsForm);

  // Trees side by side
  auto treeLayout = new QHBoxLayout;
  
  // Inbound tree
  auto inboundLayout = new QVBoxLayout;
  inboundLayout->addWidget(new QLabel(tr("Inbound Streams")));
  
  m_inboundTree = new QTreeWidget;
  m_inboundTree->setHeaderLabels({tr("Stream"), tr("Type"), tr("Channels"), tr("Rate"), tr("UID")});
  m_inboundTree->setSelectionMode(QAbstractItemView::MultiSelection);
  inboundLayout->addWidget(m_inboundTree);
  
  treeLayout->addLayout(inboundLayout, 1);
  
  // Outbound tree
  auto outboundLayout = new QVBoxLayout;
  outboundLayout->addWidget(new QLabel(tr("Outbound Sensors")));
  
  m_outboundTree = new QTreeWidget;
  m_outboundTree->setHeaderLabels({tr("Sensor Name"), tr("Data Type")});
  m_outboundTree->setSelectionMode(QAbstractItemView::SingleSelection);
  m_outboundTree->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
  connect(m_outboundTree, &QTreeWidget::itemChanged, this, &LSLProtocolSettingsWidget::on_itemChanged);
  connect(m_outboundTree, &QTreeWidget::itemSelectionChanged, this, &LSLProtocolSettingsWidget::updateOutboundButtons);
  connect(m_outboundTree, &QTreeWidget::itemDoubleClicked, this, &LSLProtocolSettingsWidget::on_itemDoubleClicked);
  outboundLayout->addWidget(m_outboundTree);
  
  // Outbound buttons
  auto outboundBtnLayout = new QHBoxLayout;
  m_addSensorBtn = new QPushButton(tr("Add Sensor"));
  m_removeSensorBtn = new QPushButton(tr("Remove Sensor"));
  m_addChannelBtn = new QPushButton(tr("Add Channel"));
  m_removeChannelBtn = new QPushButton(tr("Remove Channel"));
  
  outboundBtnLayout->addWidget(m_addSensorBtn);
  outboundBtnLayout->addWidget(m_removeSensorBtn);
  outboundBtnLayout->addWidget(m_addChannelBtn);
  outboundBtnLayout->addWidget(m_removeChannelBtn);
  outboundBtnLayout->addStretch();
  
  outboundLayout->addLayout(outboundBtnLayout);
  
  treeLayout->addLayout(outboundLayout, 1);
  
  mainLayout->addLayout(treeLayout);

  // Connect buttons
  connect(m_addSensorBtn, &QPushButton::clicked, this, &LSLProtocolSettingsWidget::on_addSensor);
  connect(m_removeSensorBtn, &QPushButton::clicked, this, &LSLProtocolSettingsWidget::on_removeSensor);
  connect(m_addChannelBtn, &QPushButton::clicked, this, &LSLProtocolSettingsWidget::on_addChannel);
  connect(m_removeChannelBtn, &QPushButton::clicked, this, &LSLProtocolSettingsWidget::on_removeChannel);

  // Update button states
  updateOutboundButtons();

  // Populate inbound tree
  populateInboundTree();
  
  // Get LSL context for discovery
  static const auto context = ossia::unique_instance<lsl_protocol::lsl_context>();
  // Set up timer for periodic updates
  auto* timer = new QTimer(this);
  connect(timer, &QTimer::timeout, this, &LSLProtocolSettingsWidget::populateInboundTree);
  timer->start(2000);
  
  // Register callback for immediate updates
  context->register_stream_callback([self = QPointer{this}]() {
    if(self)
      QMetaObject::invokeMethod(
          self, &LSLProtocolSettingsWidget::populateInboundTree, Qt::QueuedConnection);
  });
}

Device::DeviceSettings LSLProtocolSettingsWidget::getSettings() const
{
  Device::DeviceSettings settings;
  settings.name = m_name->text();
  settings.protocol = LSLProtocolFactory::static_concreteKey();
  // Update specific settings
  LSLSpecificSettings lsl_settings = m_settings;
  lsl_settings.streamTypeFilter = m_streamTypeFilter->text().toStdString();

  // Get selected streams
  lsl_settings.subscribedStreams.clear();
  for (int i = 0; i < m_inboundTree->topLevelItemCount(); ++i)
  {
    auto* item = m_inboundTree->topLevelItem(i);
    if (item->checkState(0) == Qt::Checked)
    {
      QString uid = item->text(4);
      lsl_settings.subscribedStreams.push_back(uid.toStdString());
    }
  }
  
  // Get outbound sensors from tree
  lsl_settings.outboundSensors.clear();
  for (int i = 0; i < m_outboundTree->topLevelItemCount(); ++i)
  {
    auto* sensorItem = m_outboundTree->topLevelItem(i);
    LSLSensorConfig sensor;
    sensor.streamName = sensorItem->text(0);
    sensor.dataType = sensorItem->text(1);
    
    // Get channel names from children
    for (int j = 0; j < sensorItem->childCount(); ++j)
    {
      auto* channelItem = sensorItem->child(j);
      sensor.channelNames.push_back(channelItem->text(0).toStdString());
    }
    
    lsl_settings.outboundSensors.push_back(sensor);
  }
  
  settings.deviceSpecificSettings = QVariant::fromValue(lsl_settings);
  return settings;
}

void LSLProtocolSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_name->setText(settings.name);
  
  if (settings.deviceSpecificSettings.canConvert<LSLSpecificSettings>())
  {
    m_settings = settings.deviceSpecificSettings.value<LSLSpecificSettings>();
    m_streamTypeFilter->setText(QString::fromStdString(m_settings.streamTypeFilter));

    populateInboundTree();
    populateOutboundTree();
  }
}

void LSLProtocolSettingsWidget::populateInboundTree()
{
  // Save selected UIDs
  QStringList selectedUids;
  for (int i = 0; i < m_inboundTree->topLevelItemCount(); ++i)
  {
    auto* item = m_inboundTree->topLevelItem(i);
    if (item->checkState(0) == Qt::Checked)
    {
      selectedUids.append(item->text(4));
    }
  }
  
  m_inboundTree->clear();
  
  // Get streams from context
  static const auto context = ossia::unique_instance<lsl_protocol::lsl_context>();
  auto streams = context->get_current_streams();

  for(const auto& [uid, stream] : streams)
  {
    auto* item = new QTreeWidgetItem;
    item->setText(0, QString::fromStdString(stream.name));
    item->setText(1, QString::fromStdString(stream.type));
    item->setText(2, QString::number(stream.channel_count));
    item->setText(3, QString::number(stream.nominal_srate));
    item->setText(4, QString::fromStdString(uid));

    item->setCheckState(
        0,
        (selectedUids.contains(QString::fromStdString(uid))
         || ossia::contains(m_settings.subscribedStreams, uid))
            ? Qt::Checked
            : Qt::Unchecked);

    m_inboundTree->addTopLevelItem(item);
  }

  m_inboundTree->resizeColumnToContents(0);
  m_inboundTree->resizeColumnToContents(1);
}

void LSLProtocolSettingsWidget::populateOutboundTree()
{
  m_outboundTree->clear();
  
  for (const auto& sensor : m_settings.outboundSensors)
  {
    auto* sensorItem = new QTreeWidgetItem;
    sensorItem->setText(0, sensor.streamName);
    sensorItem->setText(1, sensor.dataType);
    sensorItem->setFlags(sensorItem->flags() | Qt::ItemIsEditable);
    sensorItem->setExpanded(true);
    
    // Add channels as children
    for (const auto& channelName : sensor.channelNames)
    {
      auto* channelItem = new QTreeWidgetItem;
      channelItem->setText(0, QString::fromStdString(channelName));
      channelItem->setFlags(channelItem->flags() | Qt::ItemIsEditable);
      sensorItem->addChild(channelItem);
    }
    
    m_outboundTree->addTopLevelItem(sensorItem);
  }
  
  m_outboundTree->expandAll();
}


void LSLProtocolSettingsWidget::on_addSensor()
{
  auto* sensorItem = new QTreeWidgetItem;
  sensorItem->setText(0, tr("NewSensor"));
  sensorItem->setText(1, "float");
  sensorItem->setFlags(sensorItem->flags() | Qt::ItemIsEditable);
  sensorItem->setExpanded(true);
  
  m_outboundTree->addTopLevelItem(sensorItem);
  m_outboundTree->setCurrentItem(sensorItem);
}

void LSLProtocolSettingsWidget::on_removeSensor()
{
  auto* current = m_outboundTree->currentItem();
  if (!current)
    return;
  
  // Find top-level sensor item
  while (current->parent())
    current = current->parent();
  
  delete current;
}

void LSLProtocolSettingsWidget::on_addChannel()
{
  auto* current = m_outboundTree->currentItem();
  if (!current)
    return;
  
  // Find the sensor (top-level item)
  auto* sensorItem = current;
  while (sensorItem->parent())
    sensorItem = sensorItem->parent();
  
  // Add new channel
  auto* channelItem = new QTreeWidgetItem;
  channelItem->setText(0, QString("ch%1").arg(sensorItem->childCount() + 1));
  channelItem->setFlags(channelItem->flags() | Qt::ItemIsEditable);
  sensorItem->addChild(channelItem);
  sensorItem->setExpanded(true);
}

void LSLProtocolSettingsWidget::on_removeChannel()
{
  auto* current = m_outboundTree->currentItem();
  if (!current || !current->parent())
    return;
  
  // Only remove if it's a channel (has a parent sensor)
  if (current->parent())
  {
    delete current;
  }
}

void LSLProtocolSettingsWidget::on_itemChanged(QTreeWidgetItem* item, int column)
{
  // Simple handling - just let the user edit names and data types directly
}

void LSLProtocolSettingsWidget::on_itemDoubleClicked(QTreeWidgetItem* item, int column)
{
  if (column == 1 && !item->parent()) // Data type column for sensor
  {
    // Create combo box for data type selection
    auto* combo = new QComboBox;
    combo->addItems({"float", "int", "string"});
    combo->setCurrentText(item->text(1));
    
    connect(combo, QOverload<const QString&>::of(&QComboBox::currentTextChanged),
            [item](const QString& text) {
              item->setText(1, text);
            });
    
    m_outboundTree->setItemWidget(item, 1, combo);
    combo->showPopup();
  }
}

void LSLProtocolSettingsWidget::updateOutboundButtons()
{
  auto* current = m_outboundTree->currentItem();
  bool hasCurrent = (current != nullptr);
  bool isSensor = hasCurrent && !current->parent();
  bool isChannel = hasCurrent && current->parent();
  
  m_removeSensorBtn->setEnabled(hasCurrent);
  m_addChannelBtn->setEnabled(hasCurrent); // Can always add channel if something is selected
  m_removeChannelBtn->setEnabled(isChannel); // Can only remove channels, not sensors
}

}
